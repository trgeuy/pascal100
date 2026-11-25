#include "boards/digicomp-pascal100.h"

#include "core/statefile.h"

namespace altair {

// What the MicroEngine writes to word FC40 when it acknowledges an interrupt (manual
// ch. II F.3: "information intended for a special interrupt controller ... spurious"). The
// WD9000 data sheet says the acknowledge "presents a 4 bit code on the DAL lines which
// provides mask capability to disable other interrupt levels" (I0-I3) -- so the low four
// bits are a level code. Which level the Pascal-100 wires, and what the other bits hold, is
// not in either manual: FFFF stands in, NOT verified. A guest that keeps nothing at FC40
// (the manual's own advice) cannot tell.
static constexpr uint16_t kSpuriousFc40 = 0xFFFF;

namespace {
// Clears a flag on the way out, also when a cycle breakpoint unwinds the cycle
// (CycleBreakBefore, core/bus.h): a stale flag would misread the next monitor OUT.
struct ClearOnExit {
    bool& flag;
    ~ClearOnExit() { flag = false; }
};
} // namespace

Pascal100Board::Pascal100Board()
    : z80_(std::make_unique<CpuZ80>()), me_(std::make_unique<PMachineCore>()) {
    me_->attach(&meSide_);
}

// ---------------------------------------------------------------------------
// Running. The active processor drives the bus; the other is in HOLD (ch. IV N).
// ---------------------------------------------------------------------------
//
// A CYCLE BREAKPOINT CAN UNWIND THE STEP (CycleBreakBefore, core/bus.h): the debugger then
// puts the core's registers back and runs the instruction again. The card's own state that
// a READ changes must go back too, or the second run differs from the first: the jump-on-
// reset count (Z80Side::readData) and the MicroEngine's interrupt request (cleared by its
// acknowledge). A write never reaches the card before the veto, so nothing else moves.
StepResult Pascal100Board::step(Bus& bus) {
    const uint8_t jumpReads = jumpReads_;
    const bool meInt = meIntLatch_;
    try {
        if (!meActive_) {
            TranslatorScope scope(bus, &z80Side_);
            ClearOnExit io{z80Io_};   // set by Z80Side::ioAddress for the cycle in flight
            return z80_->step(bus);
        }
        return me_->step();
    } catch (const CycleBreakBefore&) {
        jumpReads_ = jumpReads;
        meIntLatch_ = meInt;
        throw;
    }
}

// U208B is reset by an NMI, always, and by pINT when auto-transfer is on (U237 output 3;
// ch. IV M, O). Both are levels: while one is up the MicroEngine cannot hold the bus.
void Pascal100Board::settleSwitch() {
    if (!meActive_ || !bus()) return;
    if (bus()->nmiAsserted() || (latchBit(kAutoXfer) && bus()->intPending())) meActive_ = false;
}

// Where each core fetches, for the debugger (CpuCard::peekCodeByte): the MicroEngine reads
// code byte SEGB*2 + IPC through its Map; the Z80 reads through its own Map, and right
// after a reset it gets the jump-on-reset bytes. A peek, never a cycle.
uint8_t Pascal100Board::peekCodeByte(CpuCore* core, const Bus& bus, uint32_t addr) {
    if (core == me_.get()) {
        uint32_t b = ((uint32_t)me_->segb_ * 2 + addr) & 0x1FFFF;
        if (b >= 0x1FC00) return 0xFF;   // words FE00-FFFF are the I/O window, not memory
        return bus.peekX(meByteToPhys(b));
    }
    // Jump-on-reset still pending: the next reads get C3 00 X0, counted from the Z80's PC.
    if (jumpReads_ < 3) {
        unsigned k = jumpReads_ + (uint16_t)(addr - z80_->pc());
        if (k < 3) return k == 0 ? 0xC3 : k == 1 ? 0x00 : (uint8_t)(resetAddr_ >> 8);
    }
    return bus.peekX(z80Side_.translate(addr, Cycle::MemRead));
}

// ---------------------------------------------------------------------------
// The two ports. Write-only, decoded on A0-A7 (U226 compares A1-A7, A0 picks the port):
// QB writes the System Control Port with OUT (C),A and B holds whatever it holds.
// ---------------------------------------------------------------------------
bool Pascal100Board::decodes(const BusCycle& c) const {
    return c.type == Cycle::IoWrite && (c.addr & 0xFE) == port_;
}

void Pascal100Board::write(const BusCycle& c) {
    if ((c.addr & 1) == 0) {
        writeControl(c.data);
        return;
    }
    if (meIo_) {
        // The Map RAM's entry address comes from the Z80's own address lines (ch. IV L);
        // "only the Z80 can load the Map" (ch. II D.3).
        warnOnce(saidMeMap_, "the MicroEngine wrote the Map Output Port; only the Z80 can "
                             "load the Map, so the write is ignored");
        return;
    }
    // The Z80's own A8-A15 (B on an OUT (C),A), kept before the mirror replaced them.
    // A cycle that is not the Z80's (the monitor's OUT) has only the bus's A8-A15.
    uint8_t high = z80Io_ ? z80IoHigh_ : (uint8_t)(c.addr >> 8);
    int entry = (latchBit(kMapHalf) << 3) | (high >> 5);
    map_[entry] = (uint8_t)((~c.data >> 2) & 0x0F);  // A bits 5-2, stored inverted (Fig. 2-2)
}

// ch. IV M: DO6 = 1 strobes U237 (output DO2-DO0 := DO7); DO6 = 0 strobes U208B := DO7.
void Pascal100Board::writeControl(uint8_t v) {
    bool d7 = (v & 0x80) != 0;
    if ((v & 0x40) == 0) {
        meActive_ = d7;
        return;
    }
    int n = v & 7;
    bool was = (u237_ >> n) & 1;
    u237_ = (uint8_t)(d7 ? (u237_ | (1u << n)) : (u237_ & ~(1u << n)));
    // The MicroEngine's interrupt input: C4 then 44 is a pulse that is over before the
    // MicroEngine next runs (ch. II F.3), so the request is held until it is acknowledged.
    if (n == kMeInt && d7 && !was) meIntLatch_ = true;
}

// ---------------------------------------------------------------------------
// The Z80 side (core/bus.h AddressTranslator), set only around the Z80's step.
// ---------------------------------------------------------------------------
// The Standard Map: Z80 A15-A13 pick entries 0-7 (A16 is held 0); the entry drives
// A16-A13 (ch. II D.3, ch. IV L). Off: the address goes to the bus as it is.
uint32_t Pascal100Board::Z80Side::translate(uint16_t logical, Cycle) {
    if (!card->latchBit(kZ80Map)) return logical;
    return ((uint32_t)card->map_[(logical >> 13) & 7] << 13) | (logical & 0x1FFF);
}

// The address mirror U238 (ch. IV K): the port goes out on both halves.
uint16_t Pascal100Board::Z80Side::ioAddress(uint16_t addr, Cycle) {
    card->z80IoHigh_ = (uint8_t)(addr >> 8);
    card->z80Io_ = true;
    uint8_t port = (uint8_t)addr;
    return (uint16_t)((port << 8) | port);
}

// Jump-on-reset (ch. IV P): the cycles go out; the first three reads get C3 00 X0.
uint8_t Pascal100Board::Z80Side::readData(uint8_t v) {
    if (card->jumpReads_ >= 3) return v;
    switch (card->jumpReads_++) {
    case 0: return 0xC3;
    case 1: return 0x00;
    default: return (uint8_t)(card->resetAddr_ >> 8);
    }
}

// ---------------------------------------------------------------------------
// The MicroEngine side: its WordPort (cpu/cpuPmachine.h).
// ---------------------------------------------------------------------------
// Byte address 2w of word w (17 bits); with its Map on, A16-A13 pick one of all 16 entries.
uint32_t Pascal100Board::meByteToPhys(uint32_t b) const {
    b &= 0x1FFFF;
    if (!latchBit(kMeMap)) return b;
    return ((uint32_t)map_[b >> 13] << 13) | (b & 0x1FFF);
}

// Words FE00-FFFF are I/O (ch. II E.2), decoded on the logical address, before the Map.
// FFpp is an 8-bit cycle to port pp, the port on both address halves. FEpp asks for 16
// bits; with no SIXTN "a single byte is transferred ... otherwise identical" -- and no
// board in this simulator answers SIXTN on an I/O cycle, so it is the same 8-bit cycle.
// A single-byte read strobes both data latches U128 and U140 (ch. IV I.1, the table), so
// the byte is in both halves of the word. Read from that table; not yet checked on a
// real board.
uint16_t Pascal100Board::MeSide::readWord(uint16_t w) {
    Bus* bus = card->bus();
    if (!bus) return 0xFFFF;
    if (w >= 0xFE00) {
        uint8_t pp = (uint8_t)w;
        card->meIo_ = true;
        ClearOnExit io{card->meIo_};
        uint8_t v = bus->ioRead((uint16_t)((pp << 8) | pp), StInp | StWo);
        return (uint16_t)((v << 8) | v);
    }
    uint32_t phys = card->meByteToPhys((uint32_t)w * 2);
    Transfer16 t = bus->memRead16(phys, StMemR | StWo);
    if (!t.sixtn) t = bus->byteSerialRead16(phys, /*evenFirst=*/true, StMemR | StWo);
    return (uint16_t)(t.even | (t.odd << 8));  // the low byte is the even one (ch. IX)
}

void Pascal100Board::MeSide::writeWord(uint16_t w, uint16_t v) {
    Bus* bus = card->bus();
    if (!bus) return;
    if (w >= 0xFE00) {
        uint8_t pp = (uint8_t)w;
        card->meIo_ = true;
        ClearOnExit io{card->meIo_};
        bus->ioWrite((uint16_t)((pp << 8) | pp), (uint8_t)v, StOut);  // the low half (U127)
        return;
    }
    uint32_t phys = card->meByteToPhys((uint32_t)w * 2);
    uint8_t even = (uint8_t)v, odd = (uint8_t)(v >> 8);
    if (!bus->memWrite16(phys, even, odd, 0))
        bus->byteSerialWrite16(phys, even, odd, /*evenFirst=*/true, 0);
}

// The acknowledge (ch. II F.3): the MicroEngine first writes word FC40 for an interrupt
// controller the Pascal-100 does not have, then reads the vector at word FC60 (the
// manual's "FC80" is an erratum: byte 1F8C0 / 2 = FC60). kSpuriousFc40: see its comment.
// Table C-3 takes the interrupt only at label 6, when no task is ready (reference 2.21).
uint16_t Pascal100Board::MeSide::interruptVector() {
    card->meIntLatch_ = false;
    writeWord(0xFC40, kSpuriousFc40);
    return 0xFC60;
}

// ---------------------------------------------------------------------------
// Reset and power (ch. II B): the Z80 has control, every U237 option off, the lower half
// of the Map selected. The Map keeps its contents over a reset; at power-up the real RAM
// holds anything, and here it holds zeros.
// ---------------------------------------------------------------------------
void Pascal100Board::reset(Reset r) {
    z80_->reset(r);
    me_->reset(r);  // its next activation runs the boot from word FC68 (ch. II C.3)
    meActive_ = false;
    u237_ = 0;
    meIntLatch_ = false;
    jumpReads_ = jumpOnReset_ ? 0 : 3;
    z80Io_ = false;
}

void Pascal100Board::power() {
    publishPolicy();
    map_.fill(0);
    reset(Reset::PowerOn);
}

void Pascal100Board::publishPolicy() {
    if (!clock_) return;
    clock_->setHz(clockHz_);
    clock_->setIdle(idle_);
}

void Pascal100Board::warnOnce(bool& said, const std::string& msg) {
    if (said) return;
    said = true;
    log_.push_back(msg);
}

std::vector<std::string> Pascal100Board::drainLog() {
    std::vector<std::string> out = Board::drainLog();
    for (std::string& s : log_) out.push_back(id + ": " + s);
    log_.clear();
    return out;
}

std::vector<MapEntry> Pascal100Board::ioMap() const {
    return {
        {port_, port_, "write", "System Control Port (U225)"},
        {(uint32_t)(port_ + 1), (uint32_t)(port_ + 1), "write", "Map Output Port (Z80 only)"},
    };
}

std::vector<UnitDef> Pascal100Board::units() const {
    return {{"z80", UnitKind::Cpu, meActive_ ? "(idle)" : "active"},
            {"me", UnitKind::Cpu, meActive_ ? "active" : "(idle)"}};
}

// ---------------------------------------------------------------------------
// Properties: the straps (U225, J202, U242, the U102 clock header) and the run policy.
// ---------------------------------------------------------------------------
std::vector<Property> Pascal100Board::properties() {
    std::vector<Property> p;
    {
        Property x;
        x.name = "port";
        x.help = "System Control Port (switch U225; even). The Map Output Port is the next "
                 "port. Factory setting D0";
        x.kind = Kind::Int;
        x.radix = 16;
        x.min = 0;
        x.max = 0xFE;
        x.get = [this] { return Value::ofInt(port_); };
        x.set = [this](const Value& v, std::string& err) {
            if (v.i() & 1) {
                err = "the System Control Port is even (U225 sets A1-A7)";
                return false;
            }
            port_ = (uint8_t)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "jump_on_reset";
        x.help = "Jumper J202: after a reset the Z80 jumps to reset_addr. Factory setting on";
        x.kind = Kind::Bool;
        x.get = [this] { return Value::ofBool(jumpOnReset_); };
        x.set = [this](const Value& v, std::string&) {
            jumpOnReset_ = v.b();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "reset_addr";
        x.help = "Switch U242: where the Z80 jumps after a reset, on a 4K boundary. Factory "
                 "setting 0000";
        x.kind = Kind::Int;
        x.radix = 16;
        x.min = 0;
        x.max = 0xF000;
        x.get = [this] { return Value::ofInt(resetAddr_); };
        x.set = [this](const Value& v, std::string& err) {
            if (v.i() & 0x0FFF) {
                err = "U242 sets A12-A15 only: use 0000, 1000, ... F000";
                return false;
            }
            resetAddr_ = (uint16_t)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "clock_hz";
        x.help = "The clock of both processors (header U102: 2 or 2.5 MHz in the manual, 3 MHz "
                 "measured on a real box). 0 runs flat out -- as fast as the host can.";
        x.kind = Kind::Int;
        x.radix = 10;
        x.unit = "Hz";
        x.min = 0;
        x.max = 100000000;
        x.get = [this] { return Value::ofInt(clockHz_); };
        x.set = [this](const Value& v, std::string&) {
            clockHz_ = v.i();
            publishPolicy();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "idle";
        x.help = "At full speed, rest while the guest reads an empty serial line, on "
                 "any port. On by default. The guest cannot see the rest.";
        x.kind = Kind::Bool;
        x.get = [this] { return Value::ofBool(idle_); };
        x.set = [this](const Value& v, std::string&) {
            idle_ = v.b();
            publishPolicy();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "achieved_hz";
        x.help = "LIVE: T-states per real second the run loop last reached. Show-only; 0 "
                 "until it has run.";
        x.kind = Kind::Int;
        x.radix = 10;
        x.unit = "Hz";
        x.get = [this] { return Value::ofInt(achievedHz_); };
        p.push_back(std::move(x));
    }
    return p;
}

// ---------------------------------------------------------------------------
// SNAPSHOT / RESTORE: both cores and every latch. The straps are config.
// ---------------------------------------------------------------------------
void Pascal100Board::serialize(StateWriter& w) const {
    Board::serialize(w);
    z80_->serialize(w);
    me_->serialize(w);
    w.boolean(meActive_);
    w.u8(u237_);
    for (uint8_t e : map_) w.u8(e);
    w.boolean(meIntLatch_);
    w.u8(jumpReads_);
}

void Pascal100Board::deserialize(StateReader& r) {
    Board::deserialize(r);
    z80_->deserialize(r);
    me_->deserialize(r);
    meActive_ = r.boolean();
    u237_ = r.u8();
    for (uint8_t& e : map_) e = (uint8_t)(r.u8() & 0x0F);
    meIntLatch_ = r.boolean();
    uint8_t reads = r.u8();
    jumpReads_ = reads > 3 ? 3 : reads;
    z80Io_ = false;
}

} // namespace altair
