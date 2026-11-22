#pragma once
//
// THE x86 CORE, SHARED BY THE 8088/8086 (cpu8088.cpp) AND THE 80286 (cpu80286.cpp).
// PRIVATE: only those two files include this header, because it includes CLK (third_party/clk,
// the instruction set; see cpu8088.h for what CLK is and what this file adds to it).
//
// Everything here is a template on CLK's model, so the 8086 and the 80286 share one bus
// adapter, one step(), one register list and one state format. What differs:
//
//   * ADDRESS LINES. The 8088/8086 drives A0-A19 (20 bits, wrapping at 1 MB); the 80286 drives
//     A0-A23. A card can replace A20-A23 (the CPU 85/88 and CPU 86/87 Memory Manager latch,
//     the CPU 286's memory manager through jumpers J3-J6): `high` is the value and `highMask`
//     says which of the four lines the card drives. On the 8088 the mask is all four (the
//     CPU drives none of them).
//   * THE CODE SEGMENT BASE. Fetches and the PC use CLK's CS descriptor base, not CS*16: the
//     80286 comes out of reset with CS = F000 but a hidden base of FF0000 (linear FFFFF0),
//     and keeps it until the first far jump loads CS (CPU 286 manual 451B p. 13).
#include "core/bus.h"
#include "core/statefile.h"
#include "cpu/cpu.h"
#include "cpu/timing8088.h"
#include "cpu/timing80286.h"
#include "cpu/x86local.h"

// CLK, vendored (third_party/clk, a SYSTEM include directory).
#include "InstructionSets/x86/AccessType.hpp"
#include "InstructionSets/x86/Decoder.hpp"
#include "InstructionSets/x86/Flags.hpp"
#include "InstructionSets/x86/Perform.hpp"
#include "Machines/PCCompatible/SegmentedMemory.hpp"
#include "Machines/PCCompatible/Segments.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace altair::x86core {

namespace x86 = InstructionSet::x86;

// The 8080-style status word on each cycle (core/bus.h). The x86 has no sSTACK (IEEE 696
// dropped it), so StStack is never set; an opcode fetch carries M1.
constexpr uint8_t kStRead  = StMemR | StWo;
constexpr uint8_t kStFetch = StMemR | StM1 | StWo;
constexpr uint8_t kStWrite = 0;
constexpr uint8_t kStIn    = StInp | StWo;
constexpr uint8_t kStOut   = StOut;
constexpr uint8_t kStInta  = StInta | StWo;

template <x86::Model M> constexpr uint32_t kMaxAddress = M >= x86::Model::i80286 ? (1u << 24) : (1u << 20);

// What the bus adapter needs from the core, per step.
struct Wire {
    Bus* bus = nullptr;
    uint32_t high = 0;      // A20-A23 from the card, shifted to bit 20
    uint32_t highMask = 0;  // which of A20-A23 the card drives, shifted to bit 20
    int words = 0;          // word transfers that took two bus cycles (timing: +4 clocks each)
    bool wide = false;      // a 16-bit data bus (the 8086, the 80286)
    X86LocalMemory* local = nullptr;  // the card's wait states and on-board memory, if any
};

// THE MEMORY CLK SEES. CLK asks for `access<IntT, type>(linear, base)`; reads go to the
// bus here, a byte at a time, low byte first. A write or read-modify-write gets a buffer
// instead; CLK promises one such loan at a time and calls write_back() after it, which is
// where the bytes go to the bus. Before the 286, PUSH takes its source as read-modify-write
// (for PUSH SP), so PUSH [mem] holds that loan while the push takes a second: the two
// buffers alternate, and only the newest is written (PUSH never writes its source).
template <uint32_t Max>
struct BusMemory {
    static constexpr uint32_t MaxAddress = Max;
    Wire& w;
    explicit BusMemory(Wire& wire) : w(wire) {}

    // The physical address: the CPU's lines, with the card's A20-A23 where it drives them.
    uint32_t phys(uint32_t a) const { return ((a & (MaxAddress - 1)) & ~w.highMask) | (w.high & w.highMask); }

    // One byte cycle at linear address `a`: the card's own memory, or the S-100 bus.
    uint8_t rd(uint32_t a, uint8_t status = kStRead) {
        const uint32_t p = phys(a);
        if (w.local && w.local->cycle(p, false)) return w.local->read(p);
        return w.bus->memReadX(p, status);
    }
    void wr(uint32_t a, uint8_t v) {
        const uint32_t p = phys(a);
        if (w.local && w.local->cycle(p, true)) { w.local->write(p, v); return; }
        w.bus->memWriteX(p, v, kStWrite);
    }
    // A look with no cycle (the decode look-ahead).
    uint8_t peek(uint32_t a) const {
        const uint32_t p = phys(a);
        if (w.local && w.local->owns(p)) return w.local->peek(p);
        return w.bus->peekX(p);
    }

    // The second byte of a word: the 8086 family wraps inside the segment at offset FFFF.
    static uint32_t next(uint32_t address, uint32_t base) {
        return ((address - base) & 0xFFFF) == 0xFFFF ? base : address + 1;
    }

    // THE ONE-CYCLE WORD: an even address whose odd byte is the next address (not the FFFF
    // wrap). It asks for 16 bits (sXTRQ*); a board that answers SIXTN* moves both bytes in one
    // cycle. Otherwise the card runs the byte-serial pair itself, the EVEN byte first (CPU
    // 86/87: its A0 = LA0 XOR FLIP, schematic 5/9; CPU 286 451B: CLA0, schematic 3/5), and
    // that costs the second cycle.
    bool oneCycle(uint32_t address, uint32_t base) const {
        return w.wide && (address & 1) == 0 && next(address, base) == address + 1;
    }

    // The address of the word after the one at `address` (a 32-bit value is two words).
    static uint32_t nextWord(uint32_t address, uint32_t base) { return next(next(address, base), base); }

    template <typename IntT> IntT read2(uint32_t address, uint32_t base, uint8_t status = kStRead) {
        if constexpr (sizeof(IntT) == 1) {
            return rd(address, status);
        } else if constexpr (sizeof(IntT) == 4) {
            // Only LGDT and LIDT (the base, after the limit): two words, low first.
            const uint32_t lo = read2<uint16_t>(address, base, status);
            return IntT(lo | (uint32_t(read2<uint16_t>(nextWord(address, base), base)) << 16));
        } else {
            if (oneCycle(address, base)) {
                const uint32_t p = phys(address);
                if (w.local && w.local->cycle(p, false))   // the card's 16-bit memory: one cycle
                    return IntT(w.local->read(p) | (w.local->read(p + 1) << 8));
                Transfer16 t = w.bus->memRead16(p, status);
                if (!t.sixtn) {
                    ++w.words;
                    if (w.local) (void)w.local->cycle(p + 1, false);   // the second cycle's waits
                    t = w.bus->byteSerialRead16(p, true, status);
                }
                return IntT(t.even | (t.odd << 8));
            }
            ++w.words;  // the 8088, or a word at an odd address: two byte cycles
            const uint8_t lo = rd(address, status);
            const uint8_t hi = rd(next(address, base));
            return IntT(lo | (hi << 8));
        }
    }

    void word(uint32_t address, uint32_t base, uint8_t lo, uint8_t hi) {
        if (oneCycle(address, base)) {
            const uint32_t p = phys(address);
            if (w.local && w.local->cycle(p, true)) {   // the card's 16-bit memory: one cycle
                w.local->write(p, lo);
                w.local->write(p + 1, hi);
                return;
            }
            if (!w.bus->memWrite16(p, lo, hi, kStWrite)) {
                ++w.words;
                if (w.local) (void)w.local->cycle(p + 1, true);   // the second cycle's waits
                w.bus->byteSerialWrite16(p, lo, hi, true, kStWrite);
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
        if (wbSize == 4) word(nextWord(wbAddr, wbBase), wbBase, p[2], p[3]);
        wbSize = 0;
    }

    template <typename IntT> void preauthorised_write(uint32_t address, uint32_t base, IntT value) {
        address &= MaxAddress - 1;
        if constexpr (sizeof(IntT) >= 2) word(address, base, uint8_t(value), uint8_t(value >> 8));
        else                             wr(address, uint8_t(value));
        // SGDT and SIDT store the base as 32 bits: the second word.
        if constexpr (sizeof(IntT) == 4)
            word(nextWord(address, base), base, uint8_t(value >> 16), uint8_t(value >> 24));
    }

    void preauthorise_read(uint32_t, uint32_t) {}
    void preauthorise_write(uint32_t, uint32_t) {}
    template <typename IntT> IntT read(uint32_t address) { return read2<IntT>(address, address); }
};

// I/O: a 16-bit port on A0-A15; a word is two byte cycles, port then port+1. The 8088 always
// does this. The 8086 and 80286 ask for 16 bits at an even port, and the CPU 86/87 and CPU
// 286 run the same two byte cycles when no board answers SIXTN*; no I/O board in pascal100
// does, so the bus has no 16-bit I/O cycle (docs/boards/compupro-cpu8687.md).
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

template <x86::Model M>
using SegmentsT = PCCompatible::Segments<M, BusMemory<kMaxAddress<M>>>;

template <x86::Model M>
struct FlowController {
    x86::Registers<M>& r;
    SegmentsT<M>& s;
    bool halted = false, repeat = false;
    bool jumped = false;   // a transfer of control ran (even to the next instruction)
    template <typename A> void jump(A a) {
        r.ip() = a;
        jumped = true;
    }
    template <typename A> void jump(uint16_t seg, A a) {
        jumped = true;
        s.preauthorise(x86::Source::CS, seg);
        r.cs() = seg;
        s.did_update(x86::Source::CS);
        r.ip() = a;
    }
    void halt() { halted = true; }
    void wait() {}  // TEST*/BUSY* is not wired: WAIT falls through
    void repeat_last() { repeat = true; }
    void cancel_repetition() { repeat = false; }
};

// Real mode only. An 80286 program that sets MSW.PE gets CLK's protected mode for the
// instructions, but the segment loads stay real-mode here: not supported (no pascal100 guest
// uses protected mode; MP/M 8-16 and CP/M-86 are real-mode systems).
struct CpuControl {
    x86::Mode m = x86::Mode::Real;
    void set_mode(x86::Mode x) { m = x; }
    x86::Mode mode() const { return m; }
};

// CLK's "context": everything perform() reaches, bound to the bus through Wire.
template <x86::Model M>
struct Impl {
    static constexpr auto model = M;
    static constexpr uint32_t kMax = kMaxAddress<M>;
    Wire wire;
    x86::Flags flags{M};
    x86::Registers<M> registers;
    BusMemory<kMax> linear_memory{wire};
    PCCompatible::SegmentedMemory<M, BusMemory<kMax>> memory;
    SegmentsT<M> segments;
    FlowController<M> flow_controller;
    BusIo io{wire};
    CpuControl cpu_control;
    x86::Decoder<M> decoder;

    // Core state CLK does not hold.
    bool halted = false;
    bool nmiLast = false;   // NMI* level last sampled (the edge fires)
    bool shadow = false;    // no interrupt before the next instruction (STI, MOV/POP sreg)
    bool repActive = false; // a REP string instruction is part way through
    bool refill = false;    // 80286: the last step moved CS:IP; the next instruction pays
                            // 1 clock per byte (Programmer's Reference 8.2)
    bool shutdown = false;  // 80286: stopped after an interrupt it could not deliver; only
                            // NMI or RESET goes on (Programmer's Reference 11.6)
    // The 16-bit bus fetches code a word at a time, from an even address. The last word
    // fetched: the next instruction does not fetch it again when it starts in it. Not in a
    // snapshot (the first word after RESTORE is fetched again).
    uint32_t fetchWord = 0;
    bool fetchWordValid = false;

    Impl() : memory(registers, segments, linear_memory), segments(registers, linear_memory),
             flow_controller{registers, segments} {}

    uint32_t csBase() const { return uint32_t(segments.descriptors[x86::Source::CS].base()) & (kMax - 1); }
    uint32_t linearPc() const {
        auto& R = const_cast<x86::Registers<M>&>(registers);   // CLK's accessors are not const
        return (csBase() + R.ip()) & (kMax - 1);
    }
    void resyncSegments() { segments.reset(); }
    // One segment register changed by hand: only its base follows. resyncSegments() would
    // also give CS the base CS*16, and the 80286 base after reset (FF0000) is not that.
    void reloadSegment(x86::Source s) {
        segments.descriptors[s].set_segment(registers.segment(s));
    }

    // THE 80286 SHUTS DOWN when it cannot deliver an interrupt or exception: "the 80286 will
    // shut down if the SP = 1, 3, or 5 before executing the INT or INTO instruction" (INT
    // n), and a fault while it delivers a double fault (9.6.2). It stops as HLT does; INTR
    // cannot end it, NMI or RESET can (11.6). CLK throws before it pushes anything, so the
    // registers are as they were.
    void enterShutdown() {
        shutdown = true;
        halted = true;
        repActive = false;
        fetchWordValid = false;
    }

    // An interrupt or exception, through the vector table at 0000:0000. False: the 80286
    // could not push FLAGS, CS and IP, and shut down.
    bool interrupt(uint8_t vector) {
        try {
            x86::interrupt(x86::Exception::interrupt(vector), *this);
        } catch (const x86::Exception&) {
            enterShutdown();
            return false;
        }
        halted = false;
        shutdown = false;
        repActive = false;
        fetchWordValid = false;
        return true;
    }
};

// ---------------------------------------------------------------------------
// The operations the public classes forward to.
// ---------------------------------------------------------------------------

// CS:IP exactly, as a far jump leaves them (the base follows: real mode).
template <x86::Model M> void jumpFar(Impl<M>& c, uint16_t cs, uint16_t ip) {
    c.registers.cs() = cs;
    c.registers.ip() = ip;
    c.resyncSegments();
    c.repActive = false;
    c.fetchWordValid = false;
}

// setPc keeps CS when the address is in the 64K that CS covers, and otherwise loads
// CS = address/16, IP = address%16. On the 80286, 100000-10FFEF is FFFF:0010-FFFF:FFFF (24
// address lines: real mode does not wrap at 1 MB). Returns false, changing nothing, for an
// address no real-mode CS can reach (above 10FFEF on the 80286; the caller handles it).
template <x86::Model M> bool setPc(Impl<M>& c, uint32_t v) {
    constexpr uint32_t kMask = Impl<M>::kMax - 1;
    v &= kMask;
    auto& R = c.registers;
    const uint32_t off = (v - c.csBase()) & kMask;   // modulo the address space: wraps
    if (off <= 0xFFFF) {
        R.ip() = uint16_t(off);
    } else if (v <= 0xFFFFF) {
        R.cs() = uint16_t(v >> 4);
        R.ip() = uint16_t(v & 0xF);
        c.reloadSegment(x86::Source::CS);
    } else if (M >= x86::Model::i80286 && v <= 0x10FFEF) {
        R.cs() = 0xFFFF;
        R.ip() = uint16_t(v - 0xFFFF0);
        c.reloadSegment(x86::Source::CS);
    } else {
        return false;
    }
    c.repActive = false;
    c.fetchWordValid = false;
    return true;
}

// Both resets: CS = FFFF, IP = 0, DS = SS = ES = 0, flags clear (Intel 8088 data sheet,
// "RESET"). The other registers are not changed by the chip. The 80286 caller then puts its
// own reset state on top (cpu80286.cpp).
template <x86::Model M> void reset(Impl<M>& c) {
    c.registers.reset();  // CS = FFFF, IP = 0 (the 286 model also clears MSW, sets the IDT)
    c.registers.ds() = 0;
    c.registers.ss() = 0;
    c.registers.es() = 0;
    c.flags.set(0);
    c.resyncSegments();
    c.halted = false;
    c.shadow = false;
    c.repActive = false;
    c.refill = false;
    c.shutdown = false;
    c.fetchWordValid = false;
    c.flow_controller.halted = c.flow_controller.repeat = false;
    // nmiLast is NOT reset: a line held through reset is not a new edge.
}

template <x86::Model M>
StepResult step(Impl<M>& c, Bus& bus, uint32_t high, uint32_t highMask, bool wide,
                X86LocalMemory* local = nullptr) {
    constexpr uint32_t kMask = Impl<M>::kMax - 1;
    c.wire.bus = &bus;
    c.wire.local = local;
    c.wire.high = high;
    c.wire.highMask = highMask;
    c.wire.words = 0;
    c.wire.wide = wide;

    // ---- NMI*: edge-triggered, taken whatever IF says, vector 2 ----
    const bool nmi = bus.nmiAsserted();
    const bool nmiEdge = nmi && !c.nmiLast;
    c.nmiLast = nmi;
    if (nmiEdge) {
        c.shadow = false;
        const bool taken = c.interrupt(2);   // false: shut down (again)
        if constexpr (M >= x86::Model::i80286) {
            // As INT n (23, Programmer's Reference), 2 more per word that took two cycles.
            c.refill = true;
            return {uint32_t(23 + 2 * c.wire.words), taken ? RunStatus::Ok : RunStatus::Halted};
        }
        // As INT n (50, timing8088.h) plus the 8088's 4 per word moved.
        return {uint32_t(50 + 4 * c.wire.words), RunStatus::Ok};
    }

    // ---- INTR (pINT): a level, taken when IF is set and not in a shadow ----
    // Two acknowledge cycles; the first only freezes the 8259A, the second reads the vector.
    // A shut-down 80286 ignores INTR (Programmer's Reference 11.6).
    if (!c.shadow && !c.shutdown && c.flags.template flag<x86::Flag::Interrupt>() &&
        bus.intPending()) {
        (void)bus.intAck(kStInta);
        const uint8_t vector = bus.intAck(kStInta);
        const bool taken = c.interrupt(vector);   // false: shut down
        if constexpr (M >= x86::Model::i80286) {
            // As INT n (23) plus the two INTA bus cycles, 2 clocks each (Hardware Reference
            // pp. 3-14, 3-15). Derived, not a published count.
            c.refill = true;
            return {uint32_t(27 + 2 * c.wire.words), taken ? RunStatus::Ok : RunStatus::Halted};
        }
        // Intel: 61 clocks for the INTR response on the 8086, which moves its three pushes
        // and the two vector words as single cycles; the 8088 adds 4 for each.
        return {uint32_t(61 + 4 * c.wire.words), RunStatus::Ok};
    }
    c.shadow = false;

    if (c.halted) return {4, RunStatus::Halted};

    // ---- Decode from a side-effect-free look at CS:IP, then fetch through the bus ----
    const uint16_t startIp = c.registers.ip();
    const uint16_t startCs = c.registers.cs();
    const uint32_t csBase = c.csBase();
    auto at = [&](int i) { return (csBase + uint16_t(startIp + i)) & kMask; };   // linear
    uint8_t code[16];
    for (int i = 0; i < 16; ++i) code[i] = c.linear_memory.peek(at(i));
    auto d = c.decoder.decode(code, sizeof code);
    if (d.first <= 0) {
        // Not reachable for these models (every byte string decodes); step over a byte
        // rather than hang if it ever is.
        c.registers.ip() = uint16_t(startIp + 1);
        return {4, RunStatus::Ok};
    }
    const int len = d.first;

    const bool continuing = c.repActive;
    if (!continuing) {
        if (c.wire.wide) {
            // THE 16-BIT BUS FETCHES WORDS (the 8086, the 80286): one cycle for each even
            // word the instruction's bytes are in, the first with M1. A word the previous
            // instruction ended in was fetched already. These cycles are code, not data:
            // `words` (the data transfers that took two cycles) does not count them.
            const int dataWords = c.wire.words;
            const uint16_t last = uint16_t(startIp + len - 1) & 0xFFFE;
            bool first = true;
            for (uint16_t off = startIp & 0xFFFE;; off = uint16_t(off + 2)) {
                const uint32_t lin = (csBase + off) & kMask;
                if (!(c.fetchWordValid && lin == c.fetchWord)) {
                    (void)c.linear_memory.template read2<uint16_t>(lin, csBase,
                                                                    first ? kStFetch : kStRead);
                    first = false;
                }
                c.fetchWord = lin;
                c.fetchWordValid = true;
                if (off == last) break;
            }
            c.wire.words = dataWords;
        } else {
            // The 8088: a byte per cycle; the first carries M1.
            for (int i = 0; i < len; ++i)
                (void)c.linear_memory.rd(at(i), i == 0 ? kStFetch : kStRead);
        }
    }

    const bool trap = c.flags.template flag<x86::Flag::Trap>();
    const uint16_t cx0 = c.registers.cx();
    const bool if0 = c.flags.template flag<x86::Flag::Interrupt>();

    c.registers.ip() = uint16_t(startIp + len);
    c.flow_controller.repeat = false;
    c.flow_controller.jumped = false;
    try {
        x86::perform(d.second, c, startIp);
    } catch (const x86::Exception&) {
        // The 80286 could not deliver the exception this instruction raised (CLK's fault()
        // lets the second one out): INT n with SP = 1, 3 or 5, a fault on the way to a
        // fault handler. It shuts down with CS:IP on the instruction. The time is the fetch
        // alone: Intel gives no count for the way to a shutdown.
        c.flow_controller.halted = false;
        c.flow_controller.repeat = false;
        c.enterShutdown();
        return {uint32_t(std::max(2, (len + 1) & ~1)), RunStatus::Halted};
    }
    if (c.flow_controller.jumped || c.registers.cs() != startCs) c.fetchWordValid = false;
    if (c.flow_controller.halted) {
        c.flow_controller.halted = false;
        c.halted = true;
    }

    // REP: one iteration per step. Leave IP on the instruction until CX runs out (or the
    // compare ends it), so an interrupt can come in between iterations.
    const bool repeating = c.flow_controller.repeat;
    if (repeating) {
        c.registers.ip() = startIp;
        if (c.registers.cs() != startCs) {   // not reachable: a string op never loads CS
            c.registers.cs() = startCs;
            c.resyncSegments();
        }
    }
    c.repActive = repeating;

    // The one-instruction interrupt shadow.
    int p = 0;
    while (p < len && (code[p] == 0x26 || code[p] == 0x2E || code[p] == 0x36 || code[p] == 0x3E ||
                       code[p] == 0xF0 || code[p] == 0xF2 || code[p] == 0xF3))
        ++p;
    const uint8_t op = code[p];
    // 0F is POP CS on the 8088 and 8086, a segment load; on the 80286 it starts a two-byte
    // opcode (LMSW, LGDT, ...), which loads no segment register.
    if ((op == 0xFB && !if0) || op == 0x8E || op == 0x07 || op == 0x17 || op == 0x1F ||
        (op == 0x0F && M < x86::Model::i80286))
        c.shadow = true;

    // ---- Time ----
    // A branch taken: CLK's flow controller ran a jump (a jump to the next instruction
    // counts), or CS:IP moved some other way.
    // (80286 only: the 8088 table was fitted with the CS:IP test alone.)
    const bool jumped = M >= x86::Model::i80286 && c.flow_controller.jumped;
    const bool taken = !repeating && (jumped || c.registers.cs() != startCs ||
                                      c.registers.ip() != uint16_t(startIp + len));
    int t;
    if constexpr (M >= x86::Model::i80286) {
        i80286::TimingFacts f;
        f.taken = taken;
        f.reps = c.registers.cx() != cx0 ? 1 : 0;
        f.repFirst = !continuing;
        f.shiftCount = cx0 & 0xFF;
        f.wordTransfers = c.wire.words;
        t = i80286::timing(code, len, f);
        // The queue refill after a transfer of control: 1 clock per byte of this instruction.
        if (c.refill && !continuing) t += len;
        c.refill = taken;
        // The fetch floor: the 80286 fetches a word per 2-clock bus cycle (Hardware
        // Reference p. 3-15).
        if (!continuing) t = std::max(t, (len + 1) & ~1);
    } else {
        i8088::TimingFacts f;
        f.taken = taken;
        f.reps = c.registers.cx() != cx0 ? 1 : 0;
        f.repFirst = !continuing;
        f.shiftCount = cx0 & 0xFF;
        f.wordTransfers = c.wire.words;
        t = i8088::timing(code, len, f);
        // The fetch floor (timing8088.h): the 8088 fetches a byte per 4-clock cycle, the
        // 8086 a word.
        if (!continuing) t = std::max(t, (c.wire.wide ? 2 : 4) * len);
    }

    // ---- TF: single step, INT 1 after an instruction that started with TF set ----
    if (trap && !repeating && !c.halted) {
        c.wire.words = 0;
        c.interrupt(1);
        if constexpr (M >= x86::Model::i80286) {
            t += 23 + 2 * c.wire.words;
            c.refill = true;
        } else {
            t += 50 + 4 * c.wire.words;
        }
    }

    return {uint32_t(t), c.halted ? RunStatus::Halted : RunStatus::Ok};
}

template <x86::Model M> std::vector<RegDef> registers(Impl<M>* c) {
    auto& R = c->registers;
    auto r16 = [](const char* n, const char* help, uint16_t* p, int line) {
        RegDef d{n, 16, "", RegShow::Field, help, [p] { return (uint32_t)*p; },
                 [p](uint32_t v) { *p = (uint16_t)v; }};
        d.line = line;
        return d;
    };
    // A segment register set by hand: only its own base follows (reloadSegment).
    auto seg = [c](const char* n, const char* help, uint16_t* p, x86::Source src) {
        RegDef d{n, 16, "", RegShow::Field, help, [p] { return (uint32_t)*p; },
                 [c, p, src](uint32_t v) {
                     *p = (uint16_t)v;
                     c->reloadSegment(src);
                     c->repActive = false;
                     c->fetchWordValid = false;
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
                  c->fetchWordValid = false;
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
        seg("CS", "code segment", &R.cs(), x86::Source::CS),
        seg("DS", "data segment", &R.ds(), x86::Source::DS),
        seg("SS", "stack segment", &R.ss(), x86::Source::SS),
        seg("ES", "extra segment", &R.es(), x86::Source::ES),
        ip,
        fl,
        half("AL", &R.al()), half("AH", &R.ah()), half("BL", &R.bl()), half("BH", &R.bh()),
        half("CL", &R.cl()), half("CH", &R.ch()), half("DL", &R.dl()), half("DH", &R.dh()),
    };
}

// The registers, then halted, nmiLast, shadow. The caller adds its own state after this.
template <x86::Model M> void serialize(const Impl<M>& c, StateWriter& w) {
    auto& R = const_cast<x86::Registers<M>&>(c.registers);
    for (uint16_t v : {R.ax(), R.bx(), R.cx(), R.dx(), R.sp(), R.bp(), R.si(), R.di(),
                       R.cs(), R.ds(), R.ss(), R.es(), R.ip(), c.flags.get()})
        w.u16(v);
    w.boolean(c.halted);
    w.boolean(c.nmiLast);
    w.boolean(c.shadow);
}

template <x86::Model M> void deserialize(Impl<M>& c, StateReader& r) {
    auto& R = c.registers;
    R.ax() = r.u16(); R.bx() = r.u16(); R.cx() = r.u16(); R.dx() = r.u16();
    R.sp() = r.u16(); R.bp() = r.u16(); R.si() = r.u16(); R.di() = r.u16();
    R.cs() = r.u16(); R.ds() = r.u16(); R.ss() = r.u16(); R.es() = r.u16();
    R.ip() = r.u16();
    c.flags.set(r.u16());
    c.halted = r.boolean();
    c.nmiLast = r.boolean();
    c.shadow = r.boolean();
    c.resyncSegments();
    c.fetchWordValid = false;
    // A REP part way through restarts its decode (the bytes are the same); only its base
    // time is charged again.
    c.repActive = false;
}

} // namespace altair::x86core
