#include "cpu/cpu8088.h"

#include "core/statefile.h"
#include "cpu/timing8088.h"

// CLK, vendored (third_party/clk, a SYSTEM include directory). Only this file sees it.
#include "InstructionSets/x86/AccessType.hpp"
#include "InstructionSets/x86/Decoder.hpp"
#include "InstructionSets/x86/Flags.hpp"
#include "InstructionSets/x86/Perform.hpp"
#include "Machines/PCCompatible/SegmentedMemory.hpp"
#include "Machines/PCCompatible/Segments.hpp"

#include <algorithm>

namespace altair {

namespace x86 = InstructionSet::x86;

namespace {

constexpr auto kModel = x86::Model::i8086;

// The 8080-style status word on each cycle (core/bus.h). The 8088 has no sSTACK (IEEE 696
// dropped it), so StStack is never set; an opcode fetch carries M1.
constexpr uint8_t kStRead  = StMemR | StWo;
constexpr uint8_t kStFetch = StMemR | StM1 | StWo;
constexpr uint8_t kStWrite = 0;
constexpr uint8_t kStIn    = StInp | StWo;
constexpr uint8_t kStOut   = StOut;
constexpr uint8_t kStInta  = StInta | StWo;

// What the bus adapter needs from the core, per step.
struct Wire {
    Bus* bus = nullptr;
    uint32_t high = 0;  // A20-A23, shifted
    int words = 0;      // word transfers that took two bus cycles (timing: +4 clocks each)
    bool wide = false;  // the 8086: a 16-bit data bus (Cpu8088::setWideBus)
};

// THE MEMORY CLK SEES. CLK asks for `access<IntT, type>(linear, base)`; reads go to the
// bus here, a byte at a time, low byte first. A write or read-modify-write gets a buffer
// instead; CLK promises one such loan at a time and calls write_back() after it, which is
// where the bytes go to the bus. Before the 286, PUSH takes its source as read-modify-write
// (for PUSH SP), so PUSH [mem] holds that loan while the push takes a second: the two
// buffers alternate, and only the newest is written (PUSH never writes its source).
struct BusMemory {
    static constexpr uint32_t MaxAddress = 1u << 20;
    Wire& w;
    explicit BusMemory(Wire& wire) : w(wire) {}

    uint8_t rd(uint32_t a) { return w.bus->memReadX(w.high | (a & (MaxAddress - 1)), kStRead); }
    void wr(uint32_t a, uint8_t v) { w.bus->memWriteX(w.high | (a & (MaxAddress - 1)), v, kStWrite); }

    // The second byte of a word: the 8086 family wraps inside the segment at offset FFFF.
    static uint32_t next(uint32_t address, uint32_t base) {
        return ((address - base) & 0xFFFF) == 0xFFFF ? base : address + 1;
    }

    // THE 8086's ONE-CYCLE WORD: an even address whose odd byte is the next address (not the
    // FFFF wrap). It asks for 16 bits (sXTRQ*); a board that answers SIXTN* moves both bytes
    // in one cycle. Otherwise the CPU 86/87 runs the byte-serial pair itself, the EVEN byte
    // first (its A0 = LA0 XOR FLIP, schematic 5/9), and that costs the second cycle.
    bool oneCycle(uint32_t address, uint32_t base) const {
        return w.wide && (address & 1) == 0 && next(address, base) == address + 1;
    }
    uint32_t phys(uint32_t a) const { return w.high | (a & (MaxAddress - 1)); }

    template <typename IntT> IntT read2(uint32_t address, uint32_t base) {
        if constexpr (sizeof(IntT) == 1) {
            return rd(address);
        } else {
            if (oneCycle(address, base)) {
                Transfer16 t = w.bus->memRead16(phys(address), kStRead);
                if (!t.sixtn) {
                    ++w.words;
                    t = w.bus->byteSerialRead16(phys(address), true, kStRead);
                }
                return IntT(t.even | (t.odd << 8));
            }
            ++w.words;  // the 8088, or an 8086 word at an odd address: two byte cycles
            const uint8_t lo = rd(address);
            const uint8_t hi = rd(next(address, base));
            return IntT(lo | (hi << 8));
        }
    }

    void word(uint32_t address, uint32_t base, uint8_t lo, uint8_t hi) {
        if (oneCycle(address, base)) {
            if (!w.bus->memWrite16(phys(address), lo, hi, kStWrite)) {
                ++w.words;
                w.bus->byteSerialWrite16(phys(address), lo, hi, true, kStWrite);
            }
            return;
        }
        ++w.words;
        wr(address, lo);
        wr(next(address, base), hi);
    }

    union Buf { uint8_t b; uint16_t h; uint32_t d; } bufs[2]{};
    int cur = 0;
    uint32_t wbAddr = 0, wbBase = 0;
    int wbSize = 0;  // 0 = nothing on loan

    template <typename IntT> IntT& loan(uint32_t address, uint32_t base) {
        cur ^= 1;
        wbAddr = address;
        wbBase = base;
        wbSize = sizeof(IntT);
        return *reinterpret_cast<IntT*>(&bufs[cur]);
    }

    template <typename IntT, x86::AccessType type>
    typename x86::Accessor<IntT, type>::type access(uint32_t address, uint32_t base) {
        address &= MaxAddress - 1;
        if constexpr (type == x86::AccessType::Read || type == x86::AccessType::PreauthorisedRead) {
            return read2<IntT>(address, base);
        } else if constexpr (type == x86::AccessType::Write) {
            return loan<IntT>(address, base);
        } else {
            const IntT v = read2<IntT>(address, base);
            IntT& r = loan<IntT>(address, base);
            r = v;
            return r;
        }
    }

    template <typename IntT> void write_back() {
        if (!wbSize) return;
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&bufs[cur]);
        if (wbSize >= 2) word(wbAddr, wbBase, p[0], p[1]);
        else             wr(wbAddr, p[0]);
        wbSize = 0;
    }

    template <typename IntT> void preauthorised_write(uint32_t address, uint32_t base, IntT value) {
        address &= MaxAddress - 1;
        if constexpr (sizeof(IntT) >= 2) word(address, base, uint8_t(value), uint8_t(value >> 8));
        else                             wr(address, uint8_t(value));
    }

    void preauthorise_read(uint32_t, uint32_t) {}
    void preauthorise_write(uint32_t, uint32_t) {}
    template <typename IntT> IntT read(uint32_t address) { return read2<IntT>(address, address); }
};

// I/O: a 16-bit port on A0-A15; a word is two byte cycles, port then port+1. The 8088 always
// does this. The 8086 asks for 16 bits at an even port, and the CPU 86/87 runs the same two
// byte cycles when no board answers SIXTN*; no I/O board in pascal100 does, so the bus has
// no 16-bit I/O cycle (docs/boards/compupro-cpu8687.md).
struct BusIo {
    Wire& w;
    template <typename IntT> void out(uint16_t port, IntT value) {
        w.bus->ioWrite(port, uint8_t(value), kStOut);
        if constexpr (sizeof(IntT) >= 2) {
            ++w.words;
            w.bus->ioWrite(uint16_t(port + 1), uint8_t(value >> 8), kStOut);
        }
    }
    template <typename IntT> IntT in(uint16_t port) {
        IntT v = w.bus->ioRead(port, kStIn);
        if constexpr (sizeof(IntT) >= 2) {
            ++w.words;
            v = IntT(v | (IntT(w.bus->ioRead(uint16_t(port + 1), kStIn)) << 8));
        }
        return v;
    }
};

using SegmentsT = PCCompatible::Segments<kModel, BusMemory>;

struct FlowController {
    x86::Registers<kModel>& r;
    SegmentsT& s;
    bool halted = false, repeat = false;
    template <typename A> void jump(A a) { r.ip() = a; }
    template <typename A> void jump(uint16_t seg, A a) {
        s.preauthorise(x86::Source::CS, seg);
        r.cs() = seg;
        s.did_update(x86::Source::CS);
        r.ip() = a;
    }
    void halt() { halted = true; }
    void wait() {}  // TEST* is not wired: WAIT falls through
    void repeat_last() { repeat = true; }
    void cancel_repetition() { repeat = false; }
};

struct CpuControl {
    x86::Mode m = x86::Mode::Real;
    void set_mode(x86::Mode x) { m = x; }
    x86::Mode mode() const { return m; }
};

} // namespace

// CLK's "context": everything perform() reaches, bound to the bus through Wire.
struct Cpu8088::Impl {
    static constexpr auto model = kModel;
    Wire wire;
    x86::Flags flags{kModel};
    x86::Registers<kModel> registers;
    BusMemory linear_memory{wire};
    PCCompatible::SegmentedMemory<kModel, BusMemory> memory;
    SegmentsT segments;
    FlowController flow_controller;
    BusIo io{wire};
    CpuControl cpu_control;
    x86::Decoder<kModel> decoder;

    // Core state CLK does not hold.
    bool halted = false;
    bool nmiLast = false;   // NMI* level last sampled (the edge fires)
    bool shadow = false;    // no interrupt before the next instruction (STI, MOV/POP sreg)
    bool repActive = false; // a REP string instruction is part way through

    Impl() : memory(registers, segments, linear_memory), segments(registers, linear_memory),
             flow_controller{registers, segments} {}

    uint32_t linearPc() const {
        return ((uint32_t(registers.cs()) << 4) + registers.ip()) & 0xFFFFF;
    }
    void resyncSegments() { segments.reset(); }

    // An interrupt or exception, through the vector table at 0000:0000.
    void interrupt(uint8_t vector) {
        x86::interrupt(x86::Exception::interrupt(vector), *this);
        halted = false;
        repActive = false;
    }
};

Cpu8088::Cpu8088() : impl_(std::make_unique<Impl>()) { reset(Reset::PowerOn); }
Cpu8088::~Cpu8088() = default;

uint32_t Cpu8088::pc() const { return impl_->linearPc(); }
bool Cpu8088::pcSegOff(uint16_t& seg, uint16_t& off) const {
    auto& R = const_cast<Impl&>(*impl_).registers;   // CLK's accessors are not const
    seg = R.cs();
    off = R.ip();
    return true;
}
void Cpu8088::setPc(uint32_t v) {
    v &= 0xFFFFF;
    auto& R = impl_->registers;
    const uint32_t base = uint32_t(R.cs()) << 4;
    const uint32_t off  = (v - base) & 0xFFFFF;   // modulo 1M: CS near the top wraps to 0
    if (off <= 0xFFFF) {
        R.ip() = uint16_t(off);
    } else {
        R.cs() = uint16_t(v >> 4);
        R.ip() = uint16_t(v & 0xF);
        impl_->resyncSegments();
    }
    impl_->repActive = false;
}
void Cpu8088::jumpFar(uint16_t cs, uint16_t ip) {
    impl_->registers.cs() = cs;
    impl_->registers.ip() = ip;
    impl_->resyncSegments();
    impl_->repActive = false;
}
bool Cpu8088::repeating() const { return impl_->repActive; }
bool Cpu8088::halted() const { return impl_->halted; }
bool Cpu8088::interruptsEnabled() const { return impl_->flags.flag<x86::Flag::Interrupt>(); }
bool Cpu8088::haltCanEnd(bool pint, bool eventsQueued) const {
    // NMI* ends a halt whatever IF says; but nothing here knows whether a board will pull
    // it, so count queued events as a possible wake-up, as for pINT.
    return eventsQueued || (interruptsEnabled() && pint);
}

void Cpu8088::reset(Reset) {
    // Both resets: CS = FFFF, IP = 0, DS = SS = ES = 0, flags clear (Intel 8088 data
    // sheet, "RESET"). The other registers are not changed by the chip.
    Impl& c = *impl_;
    c.registers.reset();  // CS = FFFF, IP = 0
    c.registers.ds() = 0;
    c.registers.ss() = 0;
    c.registers.es() = 0;
    c.flags.set(0);
    c.resyncSegments();
    c.halted = false;
    c.shadow = false;
    c.repActive = false;
    c.flow_controller.halted = c.flow_controller.repeat = false;
    // nmiLast is NOT reset: a line held through reset is not a new edge.
}

StepResult Cpu8088::step(Bus& bus) {
    Impl& c = *impl_;
    c.wire.bus = &bus;
    c.wire.high = high_;
    c.wire.words = 0;
    c.wire.wide = wide_;

    // ---- NMI*: edge-triggered, taken whatever IF says, vector 2 ----
    const bool nmi = bus.nmiAsserted();
    const bool nmiEdge = nmi && !c.nmiLast;
    c.nmiLast = nmi;
    if (nmiEdge) {
        c.shadow = false;
        c.interrupt(2);
        // As INT n (50, timing8088.h) plus the 8088's 4 per word moved.
        return {uint32_t(50 + 4 * c.wire.words), RunStatus::Ok};
    }

    // ---- INTR (pINT): a level, taken when IF is set and not in a shadow ----
    // Two acknowledge cycles; the first only freezes the 8259A, the second reads the vector.
    if (!c.shadow && c.flags.flag<x86::Flag::Interrupt>() && bus.intPending()) {
        (void)bus.intAck(kStInta);
        const uint8_t vector = bus.intAck(kStInta);
        c.interrupt(vector);
        // Intel: 61 clocks for the INTR response on the 8086, which moves its three pushes
        // and the two vector words as single cycles; the 8088 adds 4 for each.
        return {uint32_t(61 + 4 * c.wire.words), RunStatus::Ok};
    }
    c.shadow = false;

    if (c.halted) return {4, RunStatus::Halted};

    // ---- Decode from a side-effect-free look at CS:IP, then fetch through the bus ----
    const uint16_t startIp = c.registers.ip();
    const uint16_t startCs = c.registers.cs();
    const uint32_t csBase = uint32_t(startCs) << 4;
    uint8_t code[16];
    for (int i = 0; i < 16; ++i)
        code[i] = bus.peekX(high_ | ((csBase + uint16_t(startIp + i)) & 0xFFFFF));
    auto d = c.decoder.decode(code, sizeof code);
    if (d.first <= 0) {
        // Not reachable for the 8086 model (every byte string decodes); step over a byte
        // rather than hang if it ever is.
        c.registers.ip() = uint16_t(startIp + 1);
        return {4, RunStatus::Ok};
    }
    const int len = d.first;

    const bool continuing = c.repActive;
    if (!continuing) {
        // The instruction's bytes cross the bus as fetch cycles; the first carries M1.
        for (int i = 0; i < len; ++i)
            (void)bus.memReadX(high_ | ((csBase + uint16_t(startIp + i)) & 0xFFFFF),
                               i == 0 ? kStFetch : kStRead);
    }

    const bool trap = c.flags.flag<x86::Flag::Trap>();
    const uint16_t cx0 = c.registers.cx();
    const bool if0 = c.flags.flag<x86::Flag::Interrupt>();

    c.registers.ip() = uint16_t(startIp + len);
    c.flow_controller.repeat = false;
    x86::perform(d.second, c, startIp);
    if (c.flow_controller.halted) {
        c.flow_controller.halted = false;
        c.halted = true;
    }

    // REP: one iteration per step. Leave IP on the instruction until CX runs out (or the
    // compare ends it), so an interrupt can come in between iterations.
    const bool repeating = c.flow_controller.repeat;
    if (repeating) {
        c.registers.ip() = startIp;
        c.registers.cs() = startCs;
    }
    c.repActive = repeating;

    // The one-instruction interrupt shadow.
    int p = 0;
    while (p < len && (code[p] == 0x26 || code[p] == 0x2E || code[p] == 0x36 || code[p] == 0x3E ||
                       code[p] == 0xF0 || code[p] == 0xF2 || code[p] == 0xF3))
        ++p;
    const uint8_t op = code[p];
    if ((op == 0xFB && !if0) || op == 0x8E || op == 0x07 || op == 0x17 || op == 0x1F || op == 0x0F)
        c.shadow = true;

    // ---- Time ----
    i8088::TimingFacts f;
    f.taken = c.registers.cs() != startCs || c.registers.ip() != uint16_t(startIp + len);
    if (repeating) f.taken = false;
    f.reps = c.registers.cx() != cx0 ? 1 : 0;
    f.repFirst = !continuing;
    f.shiftCount = cx0 & 0xFF;
    f.wordTransfers = c.wire.words;
    int t = i8088::timing(code, len, f);
    // The fetch floor (timing8088.h): the 8088 fetches a byte per 4-clock cycle, the 8086
    // a word.
    if (!continuing) t = std::max(t, (c.wire.wide ? 2 : 4) * len);

    // ---- TF: single step, INT 1 after an instruction that started with TF set ----
    if (trap && !repeating && !c.halted) {
        c.wire.words = 0;
        c.interrupt(1);
        t += 50 + 4 * c.wire.words;
    }

    return {uint32_t(t), c.halted ? RunStatus::Halted : RunStatus::Ok};
}

std::vector<RegDef> Cpu8088::registers() {
    Impl* c = impl_.get();
    auto& R = c->registers;
    auto r16 = [](const char* n, const char* help, uint16_t* p, int line) {
        RegDef d{n, 16, "", RegShow::Field, help, [p] { return (uint32_t)*p; },
                 [p](uint32_t v) { *p = (uint16_t)v; }};
        d.line = line;
        return d;
    };
    auto seg = [c](const char* n, const char* help, uint16_t* p) {
        RegDef d{n, 16, "", RegShow::Field, help, [p] { return (uint32_t)*p; },
                 [c, p](uint32_t v) {
                     *p = (uint16_t)v;
                     c->resyncSegments();
                     c->repActive = false;
                 }};
        d.line = 2;
        return d;
    };
    // The status lines: the flags spelled out (OF=0 DF=0 ...) on line 0, the eight
    // general registers on line 1, the segments and IP on line 2.
    auto flag = [c](const char* n, const char* help, uint16_t bit) {
        RegDef d{n, 1, "", RegShow::Flag, help,
                 [c, bit] { return (uint32_t)((c->flags.get() & bit) ? 1 : 0); },
                 [c, bit](uint32_t v) {
                     uint16_t f = c->flags.get();
                     c->flags.set(v ? uint16_t(f | bit) : uint16_t(f & ~bit));
                 }};
        d.spelled = true;
        return d;
    };
    auto half = [](const char* n, uint8_t* p) {
        return RegDef{n, 8, "", RegShow::Off, "", [p] { return (uint32_t)*p; },
                      [p](uint32_t v) { *p = (uint8_t)v; }};
    };
    RegDef ip{"IP", 16, "", RegShow::Field, "instruction pointer (offset in CS)",
              [c] { return (uint32_t)c->registers.ip(); },
              [c](uint32_t v) {
                  c->registers.ip() = (uint16_t)v;
                  c->repActive = false;
              }};
    ip.line = 2;
    RegDef fl{"FLAGS", 16, "", RegShow::Off, "the flags word",
              [c] { return (uint32_t)c->flags.get(); },
              [c](uint32_t v) { c->flags.set((uint16_t)v); }};
    return {
        flag("OF", "overflow", 0x0800),
        flag("DF", "direction", 0x0400),
        flag("IF", "interrupt enable", 0x0200),
        flag("TF", "trap (single step)", 0x0100),
        flag("SF", "sign", 0x0080),
        flag("ZF", "zero", 0x0040),
        flag("AF", "auxiliary carry", 0x0010),
        flag("PF", "parity (even)", 0x0004),
        flag("CF", "carry", 0x0001),
        r16("AX", "accumulator", &R.ax(), 1),
        r16("BX", "base", &R.bx(), 1),
        r16("CX", "count", &R.cx(), 1),
        r16("DX", "data", &R.dx(), 1),
        r16("SP", "stack pointer", &R.sp(), 1),
        r16("BP", "base pointer", &R.bp(), 1),
        r16("SI", "source index", &R.si(), 1),
        r16("DI", "destination index", &R.di(), 1),
        seg("CS", "code segment", &R.cs()),
        seg("DS", "data segment", &R.ds()),
        seg("SS", "stack segment", &R.ss()),
        seg("ES", "extra segment", &R.es()),
        ip,
        fl,
        half("AL", &R.al()), half("AH", &R.ah()), half("BL", &R.bl()), half("BH", &R.bh()),
        half("CL", &R.cl()), half("CH", &R.ch()), half("DL", &R.dl()), half("DH", &R.dh()),
    };
}

void Cpu8088::serialize(StateWriter& w) const {
    const Impl& c = *impl_;
    auto& R = const_cast<x86::Registers<kModel>&>(c.registers);
    for (uint16_t v : {R.ax(), R.bx(), R.cx(), R.dx(), R.sp(), R.bp(), R.si(), R.di(),
                       R.cs(), R.ds(), R.ss(), R.es(), R.ip(), c.flags.get()})
        w.u16(v);
    w.boolean(c.halted);
    w.boolean(c.nmiLast);
    w.boolean(c.shadow);
    w.u8((uint8_t)(high_ >> 20));
}

void Cpu8088::deserialize(StateReader& r) {
    Impl& c = *impl_;
    auto& R = c.registers;
    R.ax() = r.u16(); R.bx() = r.u16(); R.cx() = r.u16(); R.dx() = r.u16();
    R.sp() = r.u16(); R.bp() = r.u16(); R.si() = r.u16(); R.di() = r.u16();
    R.cs() = r.u16(); R.ds() = r.u16(); R.ss() = r.u16(); R.es() = r.u16();
    R.ip() = r.u16();
    c.flags.set(r.u16());
    c.halted = r.boolean();
    c.nmiLast = r.boolean();
    c.shadow = r.boolean();
    high_ = (uint32_t)(r.u8() & 0x0F) << 20;
    c.resyncSegments();
    // A REP part way through restarts its decode (the bytes are the same); only its base
    // time is charged again.
    c.repActive = false;
}

} // namespace altair
