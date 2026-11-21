#include "boards/compupro-cpu8588.h"

#include "core/statefile.h"

namespace altair {

Cpu8588Board::Cpu8588Board()
    : c85_(std::make_unique<Cpu8085>()), c88_(std::make_unique<Cpu8088>()) {}

// ---------------------------------------------------------------------------
// Running. The processor on line drives the bus; the other is in HOLD (manual p.16).
// ---------------------------------------------------------------------------
//
// A CYCLE BREAKPOINT CAN UNWIND THE STEP (CycleBreakBefore, core/bus.h): the debugger puts
// the core back and runs the instruction again, so the power-on-jump count, which a READ
// moves, goes back too. A swap or a latch write never reaches the card before the veto.
StepResult Cpu8588Board::step(Bus& bus) {
    const uint8_t jumpReads = jumpReads_;
    try {
        if (!on88_) {
            TranslatorScope scope(bus, &side85_);
            return c85_->step(bus);
        }
        TranslatorScope scope(bus, &side88_);
        return c88_->step(bus);
    } catch (const CycleBreakBefore&) {
        jumpReads_ = jumpReads;
        throw;
    }
}

// The clock follows the processor on line: the 8085 at S4's rate, the 8088 at X2/3. Set at
// the instruction boundary, after the clock has counted the IN that made the swap.
void Cpu8588Board::settleSwitch() {
    if (publishedFor88_ != on88_) publishPolicy();
}

void Cpu8588Board::publishPolicy() {
    publishedFor88_ = on88_;
    if (!clock_) return;
    clock_->setHz(on88_ ? hz88_ : hz85_, on88_ ? kNominal88Hz : kNominal85Hz);
    clock_->setIdle(idle_);
}

// Where each core fetches, for the debugger (CpuCard::peekCodeByte). A peek, never a cycle.
uint8_t Cpu8588Board::peekCodeByte(CpuCore* core, const Bus& bus, uint32_t addr) {
    if (core == c88_.get())
        return bus.peekX(((uint32_t)(latch_ >> 4) << 20) | (addr & 0xFFFFF));
    // Power-on-jump still pending: the next reads get C3 00 <S2>, counted from the 8085's PC.
    if (jumpReads_ < 3) {
        unsigned k = jumpReads_ + (uint16_t)(addr - c85_->pc());
        if (k < 3) return k == 0 ? 0xC3 : k == 1 ? 0x00 : (uint8_t)(pojAddr_ >> 8);
    }
    return bus.peekX(((uint32_t)latch_ << 16) | (addr & 0xFFFF));
}

// ---------------------------------------------------------------------------
// The port (S3; U34 compares A0-A7). IN = processor swap, OUT = Memory Manager (p.11).
// ---------------------------------------------------------------------------
bool Cpu8588Board::decodes(const BusCycle& c) const {
    return (c.type == Cycle::IoRead || c.type == Cycle::IoWrite) && (uint8_t)c.addr == port_;
}

// "FF hex will be returned in the A register" (p.12): nothing drives DI, the bus floats.
uint8_t Cpu8588Board::read(const BusCycle&) {
    swap();
    return 0xFF;
}

void Cpu8588Board::write(const BusCycle& c) { setLatch(c.data); }

void Cpu8588Board::setLatch(uint8_t v) {
    latch_ = v;
    c88_->setHighAddress((uint8_t)(v >> 4));   // the 8088 sees only A20-A23 (p.11)
}

// PORTPULSE*: the processor on line goes into HOLD, the other comes out of it (pp.16-17).
// Coming on line: resume in place, or a reset first (5RS/8RS, U7). The 8088's first time
// needs no case of its own: it has waited in its reset state since POC*.
void Cpu8588Board::swap() {
    on88_ = !on88_;
    if (on88_) {
        if (swapReset88_) c88_->reset(Reset::Bus);
        setLatch(latch_);
    } else if (swapReset85_) {
        c85_->reset(Reset::Bus);
        if (poj_ && jor_) jumpReads_ = 0;   // RESET OUT re-arms U5b only through JOR
    }
}

// ---------------------------------------------------------------------------
// The two sides (core/bus.h AddressTranslator), set only around each core's step.
// ---------------------------------------------------------------------------
uint16_t Cpu8588Board::Side85::ioAddress(uint16_t addr, Cycle) {
    if (card->iow_ && card->bus()) card->bus()->addWaitStates(1);
    return addr;
}

uint16_t Cpu8588Board::Side88::ioAddress(uint16_t addr, Cycle) {
    if (card->iow_ && card->bus()) card->bus()->addWaitStates(1);
    return addr;
}

// Power-on-jump (p.18): the cycles go out; U26 replaces the data of the first three reads.
uint8_t Cpu8588Board::Side85::readData(uint8_t v) {
    if (card->jumpReads_ >= 3) return v;
    switch (card->jumpReads_++) {
    case 0: return 0xC3;
    case 1: return 0x00;
    default: return (uint8_t)(card->pojAddr_ >> 8);
    }
}

// ---------------------------------------------------------------------------
// Reset and power. POC* resets both processors and clears the latch. RESET* resets the
// 8085, the 8088 too with J7 A-B (the factory trace), and the latch with XAC. Either way
// CLEAR* puts the 8085 on line (U4b, U5a).
// ---------------------------------------------------------------------------
void Cpu8588Board::reset(Reset r) {
    const bool poc = r == Reset::PowerOn;
    c85_->reset(r);
    if (poc || reset88_) c88_->reset(r);
    on88_ = false;
    setLatch(poc || xac_ ? 0 : latch_);
    jumpReads_ = poj_ && (poc || jor_) ? 0 : 3;
}

void Cpu8588Board::power() {
    reset(Reset::PowerOn);
    publishPolicy();
}

std::vector<UnitDef> Cpu8588Board::units() const {
    return {{"8085", UnitKind::Cpu, on88_ ? "(idle)" : "active"},
            {"8088", UnitKind::Cpu, on88_ ? "active" : "(idle)"}};
}

std::vector<MapEntry> Cpu8588Board::ioMap() const {
    return {{port_, port_, "read/write", "IN = processor swap (reads FF), OUT = Memory Manager"}};
}

// ---------------------------------------------------------------------------
// Properties: the switches and the run policy.
// ---------------------------------------------------------------------------
std::vector<Property> Cpu8588Board::properties() {
    std::vector<Property> p;
    auto flag = [&p](const char* name, const char* help, bool& f) {
        Property x;
        x.name = name;
        x.help = help;
        x.kind = Kind::Bool;
        x.get = [&f] { return Value::ofBool(f); };
        x.set = [&f](const Value& v, std::string&) {
            f = v.b();
            return true;
        };
        p.push_back(std::move(x));
    };
    auto hz = [this, &p](const char* name, const char* help, long long& h) {
        Property x;
        x.name = name;
        x.help = help;
        x.kind = Kind::Int;
        x.radix = 10;
        x.unit = "Hz";
        x.min = 0;
        x.max = 100000000;
        x.get = [&h] { return Value::ofInt(h); };
        x.set = [this, &h](const Value& v, std::string&) {
            h = v.i();
            publishPolicy();
            return true;
        };
        p.push_back(std::move(x));
    };
    {
        Property x;
        x.name = "port";
        x.help = "Switch S3: the Memory Manager (OUT) and processor swap (IN) port, A0-A7. "
                 "CompuPro standard FD";
        x.kind = Kind::Int;
        x.radix = 16;
        x.min = 0;
        x.max = 0xFF;
        x.get = [this] { return Value::ofInt(port_); };
        x.set = [this](const Value& v, std::string&) {
            port_ = (uint8_t)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    flag("xac", "S1-2 XAC: RESET* clears the Memory Manager (power-on always does). Quick "
                "start: on",
         xac_);
    flag("iow", "S1-3 IOW: one wait state in every I/O cycle. Quick start: on", iow_);
    flag("swap_reset_8085", "S1-4 5RS: reset the 8085 each time it comes on line, instead of "
                            "resuming. Must be off with DMA devices. Quick start: off",
         swapReset85_);
    flag("swap_reset_8088", "S1-5 8RS: reset the 8088 (to FFFF0) each time it comes on line, "
                            "instead of resuming. Must be off with DMA devices. Quick start: off",
         swapReset88_);
    flag("jump_on_reset", "S1-6 JOR: power-on-jump after RESET* too, not only at power-on. "
                          "Quick start: on",
         jor_);
    flag("power_on_jump", "S1-8 POJ: the 8085 starts with a JMP to poj_addr (C3 00 page). "
                          "Off: it starts at 0000",
         poj_);
    {
        Property x;
        x.name = "poj_addr";
        x.help = "Switch S2: where the 8085's power-on-jump goes, on a 256-byte page (A8-A15)";
        x.kind = Kind::Int;
        x.radix = 16;
        x.min = 0;
        x.max = 0xFF00;
        x.get = [this] { return Value::ofInt(pojAddr_); };
        x.set = [this](const Value& v, std::string& err) {
            if (v.i() & 0xFF) {
                err = "S2 sets A8-A15 only: use a page address such as E000 or F800";
                return false;
            }
            pojAddr_ = (uint16_t)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    flag("reset_8088", "Jumper J7: RESET* resets the 8088 too (A-B, factory). Off (B-C): the "
                       "8088 is reset only at power-on",
         reset88_);
    hz("clock_hz_8085", "The 8085's clock (switch S4: 6000000 or 2000000; 5000000 with the "
                        "parts-list crystal). 0 runs flat out, timed as 6000000",
       hz85_);
    hz("clock_hz_8088", "The 8088's clock (X2/3: 8000000; 5000000 with the parts-list "
                        "crystal). 0 runs flat out, timed as 8000000",
       hz88_);
    {
        Property x;
        x.name = "idle";
        x.help = "Stand down when the guest is only polling an empty keyboard. On by default.";
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
        x.help = "LIVE: T-states per real second the run loop last reached. Read-only; 0 "
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
// SNAPSHOT / RESTORE: both cores, which is on line, the latch, the jump count.
// ---------------------------------------------------------------------------
void Cpu8588Board::serialize(StateWriter& w) const {
    Board::serialize(w);
    c85_->serialize(w);
    c88_->serialize(w);
    w.boolean(on88_);
    w.u8(latch_);
    w.u8(jumpReads_);
}

void Cpu8588Board::deserialize(StateReader& r) {
    Board::deserialize(r);
    c85_->deserialize(r);
    c88_->deserialize(r);
    on88_ = r.boolean();
    setLatch(r.u8());
    uint8_t reads = r.u8();
    jumpReads_ = reads > 3 ? 3 : reads;
    publishPolicy();
}

} // namespace altair
