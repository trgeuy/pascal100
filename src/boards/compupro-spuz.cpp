#include "boards/compupro-spuz.h"

#include "core/statefile.h"
#include "core/value.h"
#include "host/endpoint.h"
#include "host/stream.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>

namespace altair {

namespace {

EndpointResolver g_resolver;

// A board on no clock is a board in a test rig that never attached one: the board Z80 never
// catches up to anything (as on the Jade DD).
Clock& stoppedClock() {
    static Clock stopped;
    return stopped;
}

int channelOf(const std::string& unit) {
    if (unit == "a") return 0;
    if (unit == "b") return 1;
    return -1;
}

std::string noUnit(const std::string& unit) {
    return "spuz has no unit '" + unit + "' -- its serial channels are 'a' and 'b'";
}

}  // namespace

void SpuzBoard::setResolver(EndpointResolver r) { g_resolver = std::move(r); }

Clock& SpuzBoard::hostClk() const { return clock_ ? *clock_ : stoppedClock(); }

SpuzBoard::SpuzBoard() : ram_(kRamMax, 0) {
    ramBoard_.d = this;
    ramBoard_.id = "ram";
    ioBoard_.d = this;
    ioBoard_.id = "duart";
    z80Unit_.d = this;
    bbus_.attach(&ramBoard_);
    bbus_.attach(&ioBoard_);
    bclk_.setHz(kBoardHz);

    // The 2681's input port (manual p. 12, sheet 4). IP6 = the Z80's HALT* (low when halted).
    // IP5/IP4 = SB1/SB0, the host's bank (PTLATCH PB1/PB0). IP3/IP2 are not connected and read
    // high (the chip's pull-ups). IP1/IP0 = CTSB/CTSA from the two connectors, active low.
    duart_.inputPins = [this] {
        uint8_t v = 0x0C;
        if (!cpu_.halted()) v |= 0x40;
        v |= (uint8_t)((ptlatch_ & kPb) << 4);
        if (!duart_.stream(0).status().cts) v |= 0x01;
        if (!duart_.stream(1).status().cts) v |= 0x02;
        return v;
    };
    duart_.powerOn(bclk_);
    cpu_.reset(Reset::PowerOn);
    followPins();
}

SpuzBoard::~SpuzBoard() {
    // Both clocks hold lambdas with `this` in them.
    if (clock_) clock_->cancel(hostWake_);
    bclk_.cancel(duartWake_);
    bbus_.detach(&ioBoard_);
    bbus_.detach(&ramBoard_);
}

// ---------------------------------------------------------------------------
// HOST TIME -> BOARD TIME: board T = host T x 8 MHz / host hz(), the remainder carried, each
// run of host time at its own hz() (Clock::forEachRate). The same conversion as JadeDdBoard::catchUp
// (DESIGN.md 3.0.4, "One thread, and the board catches up").
// ---------------------------------------------------------------------------
void SpuzBoard::catchUp() {
    if (inCatchUp_) return;
    Clock&   h   = hostClk();
    uint64_t now = h.now();
    if (now < anchorH_) anchorH_ = now;  // a host clock this board has not seen before
    long long hz = h.hz();
    if (lastHz_ <= 0) lastHz_ = hz;
    const uint64_t bhz = (uint64_t)kBoardHz;

    auto convert = [&](uint64_t dh, long long rate) {
        uint64_t r = (uint64_t)rate;
        uint64_t q = dh / r, m = dh % r;
        uint64_t n = m * bhz + rem_;
        rem_ = n % r;
        return q * bhz + n / r;
    };

    // Each run of host T-states at one rate, oldest first (Clock::forEachRate: a CPU 8085/88
    // changes hz() at every processor swap). The remainder is in units of the rate it was
    // carried at, so a change of rate drops it.
    uint64_t adv = 0;
    h.forEachRate(anchorH_, now, [&](uint64_t dh, long long rate) {
        if (rate != lastHz_) {
            rem_    = 0;
            lastHz_ = rate;
        }
        adv += convert(dh, rate);
    });
    anchorH_ = now;
    targetB_ += adv;

    inCatchUp_ = true;
    runTo(targetB_);
    inCatchUp_ = false;

    rearmHost();
}

// Run the board Z80 to `target` board T-states. In reset it does not step. Halted, it steps
// once (an interrupt may end the HALT) and then jumps to the 2681's next event: only the
// 2681's INTRN can end a HALT (NMI* is tied high, sheet 1). The 2681's events fire from the
// board clock as it passes them (duartRefresh).
void SpuzBoard::runTo(uint64_t target) {
    while (bclk_.now() < target) {
        if (!inReset()) {
            // A breakpoint on spuz0:z80 holds the Z80 here, and its clock with it. The run
            // stops at the next host boundary (Debugger::run); the next run lets it go.
            if ((z80Unit_.armed() || z80Unit_.held()) && z80Unit_.shouldStop(cpu_.pc())) break;
            z80Unit_.record();
            StepResult s = cpu_.step(bbus_);
            bclk_.advance(s.tStates + bbus_.takeWaitStates());
            if (!cpu_.halted()) continue;
        }
        uint64_t to = target;
        if (!inReset()) {
            uint64_t e = duart_.nextEvent(bclk_);
            if (e && e < to) to = e;
        }
        if (to > bclk_.now()) bclk_.advance(to - bclk_.now());
    }
}

// The 1 ms host deadline. It runs while the Z80 can still change the VI line by itself: out
// of reset and running, or halted with a 2681 event that can wake it, or halted with ZINT*
// already low and interrupts on (an interrupt with no timed event behind it: a CTS change
// found by pump(), a host write to the IMR). Otherwise it queues nothing, so a host HLT with
// every SPUZ idle is still a final halt (Clock::queued()).
void SpuzBoard::rearmHost() {
    if (!clock_) return;
    clock_->cancel(hostWake_);
    hostWake_ = Clock::kNone;
    // A core held at a breakpoint keeps its deadline: the next run lets it go
    // (PrivateCpu::resume), and nothing else would catch the board up again.
    if (!enabled_ || inReset()) return;
    if (cpu_.halted() && duart_.nextEvent(bclk_) == 0 && !cpu_.haltCanEnd(zint_, false)) return;
    uint64_t dt = std::max<uint64_t>(1, clock_->tStatesPer(1000));
    hostWake_ = clock_->after(dt, [this] { hostWake(); });
}

// STEP spuz0:z80: one instruction of the board Z80, none of the host. The board is then
// ahead of the host, and runTo does nothing until the host passes it (DESIGN.md 3.0.4).
bool SpuzBoard::stepOne(std::string& err) {
    if (inReset()) {
        err = id + ":z80 is in reset (PTLATCH D3 = 0): write 08h to FC41h first";
        return false;
    }
    z80Unit_.record();
    StepResult s = cpu_.step(bbus_);
    bclk_.advance(s.tStates + bbus_.takeWaitStates());
    rearmHost();
    return true;
}

void SpuzBoard::hostWake() {
    hostWake_ = Clock::kNone;
    catchUp();
}

// ---------------------------------------------------------------------------
// THE 2681. Both sides reach it through here. After any access its pins may have moved.
// ---------------------------------------------------------------------------
uint8_t SpuzBoard::duartRead(int reg) {
    bool    driven = true;
    uint8_t v      = duart_.read(reg, bclk_, driven);
    duartRefresh();
    return driven ? v : 0xFF;
}

void SpuzBoard::duartWrite(int reg, uint8_t v) {
    duart_.write(reg, v, bclk_);
    duartRefresh();
}

void SpuzBoard::duartRefresh() {
    duart_.poll(bclk_);
    followPins();
    bclk_.cancel(duartWake_);
    duartWake_ = Clock::kNone;
    uint64_t e = duart_.nextEvent(bclk_);
    if (e) duartWake_ = bclk_.at(e, [this] { duartRefresh(); });
}

// The 2681's outputs, as the board wires them (manual p. 12, sheet 4, ECO 138):
//   INTRN -> ZINT*, the Z80's INT* (OP7 is disconnected on 257D/E);
//   OP6   -> the base of Q3: high (OPR bit 6 = 0, as after reset) pulls the J2 VI line;
//   OP5/OP4 -> ZB1/ZB0, the Z80's bank (all OPR bits 0 after reset: bank 3).
void SpuzBoard::followPins() {
    const uint8_t op = duart_.opPins(bclk_);
    const bool    z  = duart_.intrn(bclk_);
    if (z != zint_) {
        zint_ = z;
        ioBoard_.intChanged();
    }
    const bool vi = (op & 0x40) != 0;
    if (vi != viPulled_) {
        viPulled_ = vi;
        intChanged();
    }
    zbank_ = big_ ? (op >> 4) & 0x03 : 0;
}

// ---------------------------------------------------------------------------
// THE HOST SIDE (manual pp. 9-10, sheets 2 and 3).
// ---------------------------------------------------------------------------
bool SpuzBoard::decodes(const BusCycle& c) const {
    if (!enabled_) return false;
    switch (c.type) {
        case Cycle::IoWrite:
            if (c.addr == 0xFC40) return true;  // SLAVESEL*: every board
            if (c.addr == 0xFC41) return selected_;
            return selected_ && (c.addr & 0xFFF0) == 0xFC30;
        case Cycle::IoRead:  // FC40h and FC41h are write only (p. 9)
            return selected_ && (c.addr & 0xFFF0) == 0xFC30;
        case Cycle::MemRead:
        case Cycle::MemWrite:
            return windowHit(c.addr, c.ext);
        default:
            return false;
    }
}

// A write to FC40h reaches every SPUZ by design (p. 9). Every other port is the selected
// board's alone.
bool SpuzBoard::sharesPort(const BusCycle& c) const {
    return c.type == Cycle::IoWrite && c.addr == 0xFC40;
}

uint8_t SpuzBoard::drivesBits(const BusCycle& c) const {
    if (inReset()) return 0x00;  // the hang: nothing drives the bus
    return regDriven(c.port());
}

// Q4 pulls PHANTOM* (pin 67) from VMA*, the window compare (sheet 3: VMA* -> U15 -> Q4), so a
// RAM board under the window that honors PHANTOM* steps aside.
bool SpuzBoard::assertsPhantom(const BusCycle& c) const {
    return enabled_ && (c.type == Cycle::MemRead || c.type == Cycle::MemWrite) &&
           windowHit(c.addr, c.ext);
}

uint8_t SpuzBoard::read(const BusCycle& c) {
    if (inReset()) {
        hang(c);
        return 0xFF;
    }
    catchUp();
    holdReady((uint32_t)waits_);  // J1: the host waits for BUSACK* (pp. 6, 12-13)
    if (c.type == Cycle::IoRead) {
        const uint8_t v = duartRead(c.addr & 0x0F);
        rearmHost();  // a read can start the counter (FC3Eh), which can wake a halted Z80
        return v;
    }
    return ram_[hostOffset() | c.addr];
}

void SpuzBoard::write(const BusCycle& c) {
    if (c.type == Cycle::IoWrite && c.addr == 0xFC40) {
        writeSlaveSel(c.data);
        return;
    }
    if (c.type == Cycle::IoWrite && c.addr == 0xFC41) {
        writePtlatch(c.data);
        return;
    }
    if (inReset()) {
        hang(c);
        return;
    }
    catchUp();
    holdReady((uint32_t)waits_);
    if (c.type == Cycle::IoWrite) {
        duartWrite(c.addr & 0x0F, c.data);
        rearmHost();
        return;
    }
    ram_[hostOffset() | c.addr] = c.data;
}

bool SpuzBoard::peek(uint16_t addr, uint8_t& out) const { return peekX(addr, out); }

bool SpuzBoard::peekX(uint32_t phys, uint8_t& out) const {
    if (!enabled_ || !windowHit((uint16_t)phys, (uint8_t)(phys >> 16))) return false;
    out = ram_[hostOffset() | (phys & 0xFFFF)];
    return true;
}

// SLAVESEL* (U17, sheet 2): every board clocks its compare of D3-D0 with SW1 into VPROC.
// FC40h is not on the internal bus (p. 9), so it needs no BUSACK*, no wait and no catch-up.
void SpuzBoard::writeSlaveSel(uint8_t v) {
    const bool sel = (v & 0x0F) == select_;
    if (sel == selected_) return;
    selected_ = sel;
    decodeChanged();  // the window is the selected board's alone
}

// PTLATCH* (U28, sheet 3). Also not on the internal bus. ZRST* reaches the Z80 and the 2681
// (p. 13: "The 2681 is reset whenever the Z80 is reset").
void SpuzBoard::writePtlatch(uint8_t v) {
    catchUp();  // the board Z80 lives up to now under the old latch
    v &= 0x0F;
    if ((v & kVdma) && !(ptlatch_ & kVdma) && bus_) {
        // Only one SPUZ may assert VDMA* at a time; the manual warns of damage (p. 11).
        for (Board* b : bus_->boards()) {
            auto* o = dynamic_cast<const SpuzBoard*>(b);
            if (o && o != this && o->enabled() && (o->ptlatch_ & kVdma)) {
                log_.push_back("PTLATCH VDMA* refused: " + o->id +
                               " already has it, and two DMA boards can damage the hardware "
                               "(manual p. 11)");
                v &= (uint8_t)~kVdma;
                break;
            }
        }
    }
    const bool wasReset = inReset();
    ptlatch_ = v;
    if (!wasReset && inReset()) enterReset();
    // Released: the Z80 starts at 0000h (it was reset on the way in).
    duartRefresh();
    rearmHost();
}

void SpuzBoard::enterReset() {
    cpu_.reset(Reset::Bus);
    duart_.reset(bclk_);
}

void SpuzBoard::hang(const BusCycle& c) {
    if (bus_) bus_->requestBusHang();
    const uint64_t now = hostClk().now();
    if (now == lastHangAt_) return;  // one line per host instruction (a REP MOVS is one)
    lastHangAt_ = now;
    char what[64];
    switch (c.type) {
        case Cycle::IoRead:  std::snprintf(what, sizeof what, "IN from port %04Xh", c.addr); break;
        case Cycle::IoWrite: std::snprintf(what, sizeof what, "OUT to port %04Xh", c.addr); break;
        case Cycle::MemRead: std::snprintf(what, sizeof what, "read of %06Xh", c.phys()); break;
        default:             std::snprintf(what, sizeof what, "write to %06Xh", c.phys()); break;
    }
    // 320: the fixed text is 177 bytes, `what` up to 63, the PC up to 8 hex digits. GCC's
    // -Werror=format-truncation (the Linux CI leg) refuses a buffer that could cut it.
    char buf[320];
    std::snprintf(buf, sizeof buf,
                  "the host's %s (PC %05X) reached the SPUZ while its Z80 is in reset. A real "
                  "SPUZ hangs the S-100 bus here (manual pp. 9, 29). The cycle floated and the "
                  "run stopped. Write 08h to PTLATCH (FC41h) first.",
                  what, bus_ ? (unsigned)bus_->instrPc() : 0u);
    log_.push_back(buf);
}

bool SpuzBoard::backplaneConflict(const std::vector<Board*>& all, std::string& why) const {
    for (Board* b : all) {
        auto* o = dynamic_cast<const SpuzBoard*>(b);
        if (!o || o == this || !o->enabled() || o->select_ != select_) continue;
        why = id + " and " + o->id + " both have SPUZ select " + std::to_string(select_) +
              ", so both would answer the same window and ports -- give each SPUZ its own "
              "select (0-15, SW1 positions 5-8)";
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Lifecycle. CLR* = SLVCLR* AND pRESET* (U34, sheet 3) clears U28 (PTLATCH) and U17 (VPROC):
// after power-on or a bus reset, no board is selected and every Z80 and 2681 is in reset.
// ---------------------------------------------------------------------------
void SpuzBoard::reset(Reset) {
    catchUp();
    if (selected_) {
        selected_ = false;
        decodeChanged();
    }
    const bool wasReset = inReset();
    ptlatch_ = 0;
    if (!wasReset) enterReset();
    else duart_.reset(bclk_);
    duartRefresh();
    // The bus latches each board's VI and INT wires; followPins() reports only a change, so
    // a board that has just gone into a backplane (or been powered) reports its level here.
    ioBoard_.intChanged();
    intChanged();
    rearmHost();
}

void SpuzBoard::power() {
    std::fill(ram_.begin(), ram_.end(), (uint8_t)0);
    bclk_.power();
    duartWake_ = Clock::kNone;
    targetB_ = 0;
    rem_     = 0;
    anchorH_ = hostClk().now();
    lastHz_  = hostClk().hz();
    lastHangAt_ = UINT64_MAX;
    cpu_.reset(Reset::PowerOn);
    duart_.powerOn(bclk_);
    ptlatch_ = 0;
    reset(Reset::PowerOn);
}

void SpuzBoard::clockAttached() {
    anchorH_ = hostClk().now();
    lastHz_  = hostClk().hz();
    rem_     = 0;
    rearmHost();
}

void SpuzBoard::configChanged() {
    decodeChanged();  // `select` or `window` moved
    followPins();     // `memory` changes what OP5/OP4 select
    intChanged();     // `interrupt` moved the J2 shunt
    rearmHost();
}

// ---------------------------------------------------------------------------
// Properties: the switches and jumpers of manual pp. 4-7.
// ---------------------------------------------------------------------------
std::vector<Property> SpuzBoard::properties() {
    std::vector<Property> p;
    {
        Property x;
        x.name  = "select";
        x.help  = "The board select, SW1 positions 5-8: 0-15 (default 0). Each SPUZ in a machine "
                  "needs its own; SLAVESEL (FC40h) D3-D0 picks the board that answers";
        x.kind  = Kind::Int;
        x.radix = 10;
        x.min   = 0;
        x.max   = 15;
        x.get   = [this] { return Value::ofInt(select_); };
        x.set   = [this](const Value& v, std::string&) {
            select_ = (uint8_t)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name  = "window";
        x.help  = "Address bits A23-A16 of the 64K window, SW2: 00-FF (default 0E, the CompuPro "
                  "standard: 0E0000h-0EFFFFh)";
        x.kind  = Kind::Int;
        x.radix = 16;
        x.min   = 0x00;
        x.max   = 0xFF;
        x.get   = [this] { return Value::ofInt(window_); };
        x.set   = [this](const Value& v, std::string&) {
            window_ = (uint8_t)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name    = "memory";
        x.help    = "256k (eight 41256s, four 64K banks; default) or 64k (eight 4164s, one bank)";
        x.kind    = Kind::Enum;
        x.choices = {"256k", "64k"};
        x.get     = [this] { return Value::ofStr(big_ ? "256k" : "64k"); };
        x.set     = [this](const Value& v, std::string&) {
            big_ = v.s() == "256k";
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name  = "waits";
        x.help  = "J1: the S-100 wait states on each access to the window or the 2681: 1 (A), "
                  "2 (B, as shipped) or 3 (C)";
        x.kind  = Kind::Int;
        x.radix = 10;
        x.min   = 1;
        x.max   = 3;
        x.get   = [this] { return Value::ofInt(waits_); };
        x.set   = [this](const Value& v, std::string&) {
            waits_ = (int)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "m1_wait";
        x.help = "SW1 position 4: one wait state in each Z80 M1 cycle (on, as shipped; needed "
                 "at 6 MHz and above)";
        x.kind = Kind::Bool;
        x.get  = [this] { return Value::ofBool(m1Wait_); };
        x.set  = [this](const Value& v, std::string&) {
            m1Wait_ = v.b();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        // Not a switch: the debugger's flight recorder for the board Z80 (HISTORY CPU
        // spuz0:z80). Off by default, because the Z80 never halts under CompuPro's scheduler
        // and a record on every instruction costs on every board.
        Property x;
        x.name = "history";
        x.help = "Record the board Z80's instructions for HISTORY CPU <id>:z80 (off by default: "
                 "it costs time on every Z80 instruction)";
        x.kind = Kind::Bool;
        x.get  = [this] { return Value::ofBool(z80Unit_.historyOn()); };
        x.set  = [this](const Value& v, std::string&) {
            z80Unit_.setHistory(v.b());
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        // J2: one shunt on VI0*-VI7*. There is no pINT position.
        Property x = irqJumperProperty(
            "interrupt", "J2: the VI line that 2681 OP6 pulls through Q3 (vi0, as shipped)", irq_);
        x.choices.erase(std::remove(x.choices.begin(), x.choices.end(), "int"), x.choices.end());
        p.push_back(std::move(x));
    }
    return p;
}

std::vector<std::string> SpuzBoard::statusLines() const {
    const char* state = inReset() ? "in reset" : cpu_.halted() ? "halted" : "running";
    char buf[160];
    std::snprintf(buf, sizeof buf,
                  "board Z80  PC %04X  %s  Z80 bank %d  host bank %d  %s  VI %s", cpu_.pc(),
                  state, zbank_, big_ ? (ptlatch_ & kPb) : 0,
                  selected_ ? "selected" : "not selected", viPulled_ ? "pulled" : "released");
    return {buf};
}

std::vector<MapEntry> SpuzBoard::ioMap() const {
    return {
        {0xFC30, 0xFC3F, "read/write", "2681 DUART registers 0-F (the selected board)"},
        {0xFC40, 0xFC40, "write", "SLAVESEL: D3-D0 select a board (every SPUZ)"},
        {0xFC41, 0xFC41, "write", "PTLATCH: D3 ZRST*, D2 VDMA*, D1-D0 host bank (the selected board)"},
    };
}

std::vector<MapEntry> SpuzBoard::memMap() const {
    const uint32_t lo = (uint32_t)window_ << 16;
    return {{lo, lo + 0xFFFF, "SPUZ window", "the selected board's RAM, host bank (PTLATCH D1-D0)"}};
}

// ---------------------------------------------------------------------------
// The serial channels. CompuPro's software does not use them; the chip has them (§3.0.4).
// ---------------------------------------------------------------------------
std::vector<UnitDef> SpuzBoard::units() const {
    const char* state = inReset() ? "in reset" : cpu_.halted() ? "halted" : "running";
    return {
        {"z80", UnitKind::Cpu, state},
        {"a", UnitKind::Serial, duart_.stream(0).describe()},
        {"b", UnitKind::Serial, duart_.stream(1).describe()},
    };
}

std::vector<Property> SpuzBoard::boardUnitProperties(const std::string& unit) {
    const int ch = channelOf(unit);
    if (ch < 0) return {};
    return duart_.channelProperties(
        ch, rebasingResolver(g_resolver, [this](const std::string& p) { return resolvePath(p); }));
}

bool SpuzBoard::connect(const std::string& unit, const std::string& ep, std::string& err) {
    const int ch = channelOf(unit);
    if (ch < 0) {
        err = noUnit(unit);
        return false;
    }
    if (!g_resolver) {
        err = "no endpoint resolver installed";
        return false;
    }
    std::vector<std::string> paths;
    std::string spec = rebaseEndpointPaths(ep, [&](const std::string& p) {
        paths.push_back(p);
        return resolvePath(p);
    });
    auto s = g_resolver(spec, err);
    if (!s) {
        for (const std::string& p : paths) err += pathNote(p);
        return false;
    }
    duart_.connect(ch, std::move(s));
    duartRefresh();
    return true;
}

bool SpuzBoard::connectStream(const std::string& unit, std::unique_ptr<ByteStream> s,
                              std::string& err) {
    const int ch = channelOf(unit);
    if (ch < 0) {
        err = noUnit(unit);
        return false;
    }
    duart_.connect(ch, std::move(s));
    duartRefresh();
    return true;
}

bool SpuzBoard::disconnect(const std::string& unit, std::string& err) {
    const int ch = channelOf(unit);
    if (ch < 0) {
        err = noUnit(unit);
        return false;
    }
    duart_.disconnect(ch);
    duartRefresh();
    return true;
}

ByteStream* SpuzBoard::unitStream(const std::string& unit) {
    const int ch = channelOf(unit);
    return ch < 0 ? nullptr : &duart_.stream(ch);
}

// A byte that arrived on a line is a 2681 event the chip did not know of: re-arm.
void SpuzBoard::pump() {
    duart_.pump();
    duartRefresh();
    rearmHost();
}

std::vector<std::string> SpuzBoard::drainLog() {
    std::vector<std::string> out = std::move(log_);
    log_.clear();
    for (auto& s : duart_.drainLog()) out.push_back(std::move(s));
    for (auto& s : out) s = id + ": " + s;
    return out;
}

// ---------------------------------------------------------------------------
// SNAPSHOT/RESTORE (DESIGN.md 13): the whole private machine and where board time stands
// against host time. The switches do not travel, nor do the line endpoints (host resources).
// ---------------------------------------------------------------------------
void SpuzBoard::serialize(StateWriter& w) const {
    Board::serialize(w);
    w.boolean(selected_);
    w.u8(ptlatch_);
    w.u64(anchorH_);
    w.u64(rem_);
    w.u64((uint64_t)lastHz_);
    w.u64(targetB_);
    w.raw(ram_.data(), ram_.size());
    bclk_.serialize(w);
    cpu_.serialize(w);
    duart_.serialize(w);
}

void SpuzBoard::deserialize(StateReader& r) {
    Board::deserialize(r);
    selected_ = r.boolean();
    ptlatch_  = r.u8();
    anchorH_  = r.u64();
    rem_      = r.u64();
    lastHz_   = (long long)r.u64();
    targetB_  = r.u64();
    r.raw(ram_.data(), ram_.size());
    bclk_.deserialize(r);  // empties the board clock's queue; duartRefresh() re-arms it
    duartWake_ = Clock::kNone;
    cpu_.deserialize(r);
    duart_.deserialize(r);
    lastHangAt_ = UINT64_MAX;
    duartRefresh();
    ioBoard_.intChanged();
    decodeChanged();
    intChanged();
    rearmHost();
}

} // namespace altair
