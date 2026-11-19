#include "boards/compupro-cpu8687.h"

#include "core/clock.h"
#include "core/statefile.h"

#include <cstdio>
#include <string>
#include <utility>

namespace altair {

CompuProCpu8687Board::CompuProCpu8687Board() : core_(std::make_unique<Cpu8088>()) {
    core_->setWideBus(true);   // the 8086
}

// ---------------------------------------------------------------------------
// Running. One processor, on a 16-bit bus. A cycle breakpoint can unwind the step
// (CycleBreakBefore, core/bus.h); nothing on this card moves before the veto, because
// the local ports act only after the bus cycle has run.
// ---------------------------------------------------------------------------
StepResult CompuProCpu8687Board::step(Bus& bus) {
    TranslatorScope scope(bus, &side_);
    return core_->step(bus);
}

void CompuProCpu8687Board::publishPolicy() {
    if (!clock_) return;
    clock_->setHz(hz_, kNominalHz);
    clock_->setIdle(idle_);
}

// Where the 8086 fetches, for the debugger: A0-A19 from the core, A20-A23 from U46.
uint8_t CompuProCpu8687Board::peekCodeByte(CpuCore*, const Bus& bus, uint32_t addr) {
    return bus.peekX(((uint32_t)(mm_ >> 4) << 20) | (addr & 0xFFFFF));
}

// ---------------------------------------------------------------------------
// The card's own ports (core/bus.h AddressTranslator::ownsIo).
// ---------------------------------------------------------------------------

// The 80130's ports, as the manual names them: FFF0/FFF2 interrupt controller, FFF8/FFFA/
// FFFC timers, FFFE timer control (pp. 11-18). The odd ports and +4/+6 are not described;
// they are left to the bus.
bool CompuProCpu8687Board::is80130(uint16_t addr) const {
    if (!io80130_ || (addr & 0xFFF0) != port80130_) return false;
    switch (addr & 0x0F) {
    case 0x0: case 0x2: case 0x8: case 0xA: case 0xC: case 0xE: return true;
    default: return false;
    }
}

// The timers count the CPU clock (the 80130's CLK pin is the 8086's, schematic 9/9): the
// T-states this card's core has run, which is exact flat out and paced alike.
uint64_t CompuProCpu8687Board::timerNow() const { return clock_ ? clock_->now() : 0; }

// U46: D7-D4 of the byte (AD15-AD12 for an OUT to the odd port FD) = A23-A20.
void CompuProCpu8687Board::setMm(uint8_t v) {
    mm_ = (uint8_t)(v & 0xF0);
    core_->setHighAddress((uint8_t)(mm_ >> 4));
}

// S3-3 + S3-4: one wait state on every I/O cycle, the card's own ports too.
uint16_t CompuProCpu8687Board::Side::ioAddress(uint16_t addr, Cycle) {
    if (card->ioWait_ && card->bus()) card->bus()->addWaitStates(1);
    return addr;
}

bool CompuProCpu8687Board::Side::ownsIo(uint16_t addr, Cycle t) const {
    if (t == Cycle::IoWrite && card->isMm(addr)) return true;   // the latch is write-only
    return card->is80130(addr);
}

uint8_t CompuProCpu8687Board::Side::localRead(uint16_t addr) {
    if (!card->is80130(addr)) return 0xFF;   // an IN from the Memory Manager: nothing drives
    const uint8_t live = card->bus() ? card->bus()->viLines() : 0;  // IR0-IR7 = VI0*-VI7*
    switch (addr & 0x0F) {
    case 0x0: return card->pic_.read(false, live);   // IRR or ISR (OCW3)
    case 0x2: return card->pic_.read(true, live);    // the mask
    case 0x8: return card->pit_.readCounter(0, card->timerNow());
    case 0xA: return card->pit_.readCounter(1, card->timerNow());
    case 0xC: return card->pit_.readCounter(2, card->timerNow());
    default: return 0xFF;                            // FFFE: the control port is write-only
    }
}

void CompuProCpu8687Board::Side::localWrite(uint16_t addr, uint8_t v) {
    if (card->isMm(addr)) {
        card->setMm(v);
        return;
    }
    switch (addr & 0x0F) {
    case 0x0: card->pic_.write(false, v); break;   // ICW1, OCW2, OCW3
    case 0x2: card->pic_.write(true, v); break;    // ICW2, ICW4, OCW1
    case 0x8: card->pit_.writeCounter(0, v, card->timerNow()); break;
    case 0xA: card->pit_.writeCounter(1, v, card->timerNow()); break;
    case 0xC: card->pit_.writeCounter(2, v, card->timerNow()); break;
    case 0xE: {
        // THE MODES ARE HARD-WIRED (p. 17): timer 0 mode 2, timer 1 mode 0, timer 2 mode 3,
        // whatever bits 3-1 say. The manual's own init byte for timer 0, 3AH, has mode 5 in
        // them. A latch command (bits 5-4 = 00) passes as it is.
        const int sc = v >> 6;
        if (sc == 3) break;   // no fourth timer (an 8254 read-back): not described
        if ((v & 0x30) != 0) {
            static constexpr uint8_t kMode[3] = {2, 0, 3};
            v = (uint8_t)((v & 0xF1) | (kMode[sc] << 1));
        }
        card->pit_.writeControl(v, card->timerNow());
        break;
    }
    default: break;
    }
}

// ---------------------------------------------------------------------------
// Reset and power. RESET (and POC*) resets the 8086 and clears U46 (CLEAR*, schematic 5/9).
// The 80130 has no reset pin: RESET leaves it as it is. POC* also arms the power-on jump;
// RESET arms it only with S5-9.
// ---------------------------------------------------------------------------
void CompuProCpu8687Board::reset(Reset r) {
    core_->reset(r);
    setMm(0);
    if (poj_ && (r == Reset::PowerOn || jor_)) core_->jumpFar((uint16_t)(pojAddr_ >> 4), 0);
}

void CompuProCpu8687Board::power() {
    pic_.powerOn();
    pit_.powerOn();
    reset(Reset::PowerOn);
    publishPolicy();
}

std::vector<UnitDef> CompuProCpu8687Board::units() const {
    return {{"8086", UnitKind::Cpu, "active"}};
}

// The card's own ports. Not bus decodes (they use A8-A15): the card claims them for its
// own 8086 (AddressTranslator::ownsIo), and SHOW BUS lists them here.
std::vector<MapEntry> CompuProCpu8687Board::ioMap() const {
    std::vector<MapEntry> m;
    char port[8];
    std::snprintf(port, sizeof port, "%04X", (unsigned)mmPort_);
    m.push_back({mmPort_, mmPort_, "write",
                 std::string("Memory Manager at ") + port +
                     ": D7-D4 = A23-A20 (the card's own port, all of A0-A15)"});
    if (io80130_)
        m.push_back({port80130_, (uint32_t)port80130_ + 0x0F, "read/write",
                     "80130: interrupt controller +0/+2, timers +8/+A/+C, control +E (the "
                     "card's own ports, A0-A15)"});
    return m;
}

std::string CompuProCpu8687Board::describe80130() const {
    const uint8_t live = bus() ? bus()->viLines() : 0;
    return pic_.describe(live) + "; " + pit_.describe(timerNow());
}

// ---------------------------------------------------------------------------
// Properties: the switches and the run policy.
// ---------------------------------------------------------------------------
std::vector<Property> CompuProCpu8687Board::properties() {
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
    {
        Property x;
        x.name = "mm_port";
        x.help = "Switch S4-3..10: the Memory Manager port. A15-A8 are set, A7-A0 are FD. "
                 "CompuPro standard 00FD";
        x.kind = Kind::Int;
        x.radix = 16;
        x.min = 0;
        x.max = 0xFFFF;
        x.get = [this] { return Value::ofInt(mmPort_); };
        x.set = [this](const Value& v, std::string& err) {
            if ((v.i() & 0xFF) != 0xFD) {
                err = "the low byte of the Memory Manager port is wired to FD: use xxFD, "
                      "such as 00FD";
                return false;
            }
            mmPort_ = (uint16_t)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "port_80130";
        x.help = "Switches S2 and S4-1,2: the 80130's 16 ports, on a 16-port boundary "
                 "(A15-A4). CompuPro standard FFF0";
        x.kind = Kind::Int;
        x.radix = 16;
        x.min = 0;
        x.max = 0xFFF0;
        x.get = [this] { return Value::ofInt(port80130_); };
        x.set = [this](const Value& v, std::string& err) {
            if (v.i() & 0x0F) {
                err = "the 80130 takes 16 ports on a 16-port boundary: use xxx0, such as FFF0";
                return false;
            }
            port80130_ = (uint16_t)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    flag("io_80130", "S3-2: the 80130's ports answer. CompuPro standard on, so the system "
                     "can mask its interrupts",
         io80130_);
    flag("io_wait", "S3-3 and S3-4: one wait state on every I/O cycle. CompuPro standard on",
         ioWait_);
    flag("power_on_jump", "S5-10: start at poj_addr (a jammed JMPF) instead of FFFF0. "
                          "Standard off",
         poj_);
    flag("jump_on_reset", "S5-9: the power-on jump on every RESET too, not only at power-on",
         jor_);
    {
        Property x;
        x.name = "poj_addr";
        x.help = "Switch S5-1..8 (A19-A12): where the power-on jump goes, a 4K boundary in the "
                 "low megabyte. All OFF = FF000";
        x.kind = Kind::Int;
        x.radix = 16;
        x.min = 0;
        x.max = 0xFF000;
        x.get = [this] { return Value::ofInt(pojAddr_); };
        x.set = [this](const Value& v, std::string& err) {
            if (v.i() & 0xFFF) {
                err = "S5 sets A19-A12 only: use a 4K boundary such as FF000 or 0D000";
                return false;
            }
            pojAddr_ = (uint32_t)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "clock_hz";
        x.help = "The 8086's clock: 8000000 or 10000000 as shipped, 5000000 with an 8087. "
                 "0 runs flat out, timed as 8000000";
        x.kind = Kind::Int;
        x.radix = 10;
        x.unit = "Hz";
        x.min = 0;
        x.max = 100000000;
        x.get = [this] { return Value::ofInt(hz_); };
        x.set = [this](const Value& v, std::string&) {
            hz_ = v.i();
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
// SNAPSHOT / RESTORE: the 8086, U46 and the 80130.
// ---------------------------------------------------------------------------
void CompuProCpu8687Board::serialize(StateWriter& w) const {
    Board::serialize(w);
    core_->serialize(w);
    w.u8(mm_);
    pic_.serialize(w);
    pit_.serialize(w, timerNow());
}

void CompuProCpu8687Board::deserialize(StateReader& r) {
    Board::deserialize(r);
    core_->deserialize(r);
    setMm(r.u8());
    pic_.deserialize(r);
    pit_.deserialize(r, timerNow());
    publishPolicy();
}

} // namespace altair
