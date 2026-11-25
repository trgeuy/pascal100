#include "boards/compupro-cpu286.h"

#include "core/clock.h"
#include "core/hex.h"
#include "core/roms.h"
#include "core/statefile.h"

#include <cctype>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

namespace altair {

CompuProCpu286Board::CompuProCpu286Board() : core_(std::make_unique<Cpu80286>()) {
    core_->setLocalMemory(&side_);
    setMm(0);
}

// ---------------------------------------------------------------------------
// Running. One processor, on a 16-bit bus. A cycle breakpoint can unwind the step
// (CycleBreakBefore, core/bus.h); nothing on this card moves before the veto, because
// the local ports act only after the bus cycle has run.
// ---------------------------------------------------------------------------
StepResult CompuProCpu286Board::step(Bus& bus) {
    TranslatorScope scope(bus, &side_);
    return core_->step(bus);
}

void CompuProCpu286Board::publishPolicy() {
    if (!clock_) return;
    clock_->setHz(hz_, kNominalHz);
    clock_->setIdle(idle_);
}

uint32_t CompuProCpu286Board::physical(uint32_t linear) const {
    const uint32_t mask = (uint32_t)mmLines_ << 20;
    return ((linear & 0xFFFFFF) & ~mask) | (((uint32_t)mm_ << 20) & mask);
}

// Where the 80286 fetches, for the debugger: the sockets or the bus.
uint8_t CompuProCpu286Board::peekCodeByte(CpuCore*, const Bus& bus, uint32_t addr) {
    const uint32_t p = physical(addr);
    return inSockets(p) ? side_.peek(p) : bus.peekX(p);
}

// The latch drives the lines whose jumper is A-C; the 80286 drives the rest.
void CompuProCpu286Board::setMm(uint8_t v) {
    mm_ = (uint8_t)(v & 0x0F);
    core_->setHighAddress(mm_, mmLines_);
}

// ---------------------------------------------------------------------------
// The card's own ports (core/bus.h AddressTranslator::ownsIo): 00F8-00FF and 01F8-01FF,
// all of A0-A15, unless J2 is fitted.
// ---------------------------------------------------------------------------
bool CompuProCpu286Board::isOwnPort(uint16_t addr) const {
    return !j2_ && (addr & 0xFEF8) == 0x00F8;
}

// J1: B, E, C, F and H add wait states to every I/O cycle, the card's own ports too.
uint16_t CompuProCpu286Board::Side::ioAddress(uint16_t addr, Cycle) {
    const CompuProCpu286Board& c = *card;
    const uint32_t n = (c.wait('B') ? 1 : 0) + (c.wait('E') ? 2 : 0) + (c.wait('C') ? 1 : 0) +
                       (c.wait('F') ? 2 : 0) + (c.wait('H') ? 1 : 0);
    if (n && c.bus()) c.bus()->addWaitStates(n);
    return addr;
}

bool CompuProCpu286Board::Side::ownsIo(uint16_t addr, Cycle) const { return card->isOwnPort(addr); }

// No 80287, and the memory manager is write-only: every read floats.
uint8_t CompuProCpu286Board::Side::localRead(uint16_t) { return 0xFF; }

// A byte to an even port 01F8-01FE: D0-D3 into the latch. The odd ports: see the header.
void CompuProCpu286Board::Side::localWrite(uint16_t addr, uint8_t v) {
    if ((addr & 0xFFF8) == 0x01F8 && (addr & 1) == 0) card->setMm(v);
}

// ---------------------------------------------------------------------------
// Memory: J1's wait states on every cycle, and the sockets (J7) in page FF.
// ---------------------------------------------------------------------------
bool CompuProCpu286Board::inSockets(uint32_t phys) const {
    return sockets_ && !rom_.empty() && (phys >> 16) == 0xFF;
}

bool CompuProCpu286Board::Side::cycle(uint32_t phys, bool write) {
    const CompuProCpu286Board& c = *card;
    const bool own = c.inSockets(phys);   // the sockets are part of memory space (p. 6)
    const uint32_t n = (c.wait('A') ? 1 : 0) + (c.wait('D') ? 2 : 0) + (c.wait('C') ? 1 : 0) +
                       (c.wait('F') ? 2 : 0) + (write && c.wait('H') ? 1 : 0) +
                       (own && c.wait('G') ? 4 : 0);
    if (n && c.bus()) c.bus()->addWaitStates(n);
    return own;
}

bool CompuProCpu286Board::Side::owns(uint32_t phys) const { return card->inSockets(phys); }

uint8_t CompuProCpu286Board::Side::peek(uint32_t phys) const {
    const auto& rom = card->rom_;
    return rom.empty() ? 0xFF : rom[(phys & 0xFFFF) % rom.size()];
}

// ---------------------------------------------------------------------------
// The sockets' image. `builtin:<name>` travels the ROM registry; a file is Intel HEX,
// S-records or a flat binary. Offset 0 = the first byte of the window; the image is
// rounded up to the EPROM pair that holds it (16K, 32K or 64K) and repeats through page FF.
// An empty spec empties the sockets.
// ---------------------------------------------------------------------------
bool CompuProCpu286Board::loadRom(const std::string& spec, std::string& err) {
    if (spec.empty()) {
        rom_.clear();
        return true;
    }
    Image img;
    if (spec.rfind("builtin:", 0) == 0) {
        const BuiltinRom* rom = findRom(spec.substr(8));
        if (!rom) {
            err = "no built-in ROM named '" + spec.substr(8) + "'. SHOW ROMS lists them.";
            return false;
        }
        if (!decodeRom(*rom, 0, img, err)) return false;
    } else {
        std::ifstream f(resolvePath(spec), std::ios::binary);
        if (!f) {
            err = "cannot open '" + spec + "'" + pathNote(spec);
            return false;
        }
        std::vector<uint8_t> raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (looksLikeHex(raw)) {
            if (!loadHex(raw, img, err)) { err = spec + ": " + err; return false; }
        } else if (looksLikeSrec(raw)) {
            if (!loadSrec(raw, img, err)) { err = spec + ": " + err; return false; }
        } else {
            loadBin(raw, 0, img);
        }
    }
    if (img.empty()) {
        err = spec + ": no bytes";
        return false;
    }
    const uint32_t last = img.bytes.rbegin()->first;
    if (last > 0xFFFF) {
        err = spec + ": runs past FFFF. The sockets hold 64K at most (two 27256s).";
        return false;
    }
    const size_t size = last < 0x4000 ? 0x4000 : last < 0x8000 ? 0x8000 : 0x10000;
    rom_.assign(size, 0xFF);
    for (const auto& [a, b] : img.bytes) rom_[a] = b;
    return true;
}

// ---------------------------------------------------------------------------
// Reset and power. RESET (and POC*) resets the 80286 and clears the memory manager (p. 13).
// ---------------------------------------------------------------------------
void CompuProCpu286Board::reset(Reset r) {
    core_->reset(r);
    setMm(0);
}

void CompuProCpu286Board::power() {
    std::string err;
    if (!loadRom(romSpec_, err)) log_.push_back(err);
    reset(Reset::PowerOn);
    publishPolicy();
}

std::vector<UnitDef> CompuProCpu286Board::units() const {
    return {{"80286", UnitKind::Cpu, "active"}};
}

// The card's own ports. Not bus decodes (they use A8-A15): the card claims them for its
// own 80286 (AddressTranslator::ownsIo), and SHOW BUS lists them here.
std::vector<MapEntry> CompuProCpu286Board::ioMap() const {
    std::vector<MapEntry> m;
    if (j2_) return m;
    m.push_back({0x00F8, 0x00FF, "read/write",
                 "80287 ports (no 80287 fitted: reads float; the board's own ports, A0-A15)"});
    m.push_back({0x01F8, 0x01FF, "write",
                 "Memory Manager: a byte to an even port, D0-D3 = A20-A23 (the card's own "
                 "ports, A0-A15)"});
    return m;
}

// ---------------------------------------------------------------------------
// Properties: the jumpers and the run policy.
// ---------------------------------------------------------------------------
std::vector<Property> CompuProCpu286Board::properties() {
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
        x.name = "mm_lines";
        x.help = "Jumpers J3-J6: which of A20-A23 the Memory Manager drives (bit 0 = A20 = J6 "
                 "... bit 3 = A23 = J3); the 80286 drives the others. As shipped F (all A-C): "
                 "the first fetch after reset is at 0FFFF0. 0 (all B-C): at FFFFF0";
        x.kind = Kind::Int;
        x.radix = 16;
        x.min = 0;
        x.max = 0xF;
        x.get = [this] { return Value::ofInt(mmLines_); };
        x.set = [this](const Value& v, std::string&) {
            mmLines_ = (uint8_t)v.i();
            setMm(mm_);
            return true;
        };
        p.push_back(std::move(x));
    }
    flag("j2", "J2 fitted: the 80287 ports (00F8-00FF) and the Memory Manager (01F8-01FF) go "
               "back to the S-100 bus, and the Memory Manager cannot be written. Standard off",
         j2_);
    {
        Property x;
        x.name = "sockets";
        x.help = "J7 fitted: page FF (FF0000-FFFFFF) is the on-board sockets, holding `rom`. "
                 "As shipped off: page FF is memory on the S-100 bus";
        x.kind = Kind::Bool;
        x.get = [this] { return Value::ofBool(sockets_); };
        x.set = [this](const Value& v, std::string&) {
            sockets_ = v.b();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "rom";
        x.help = "The EPROM pair in U37/U38, as one 16-bit image: Intel HEX, S-records, a flat "
                 "binary or builtin:<name>. Offset 0 = FF0000; 16K, 32K or 64K, repeated "
                 "through page FF. Empty = no chips";
        x.kind = Kind::Str;
        x.get = [this] { return Value::ofStr(romSpec_); };
        x.set = [this](const Value& v, std::string& err) {
            if (!loadRom(v.s(), err)) return false;
            romSpec_ = v.s();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "waits";
        x.help = "Jumper J1, the positions fitted (additive): A 1 memory, B 1 I/O, C 1 all, "
                 "D 2 memory, E 2 I/O, F 2 all, G 4 sockets, H 1 memory write + 1 I/O. Empty "
                 "= none (an illegal S-100 cycle). Default B";
        x.kind = Kind::Str;
        x.get = [this] { return Value::ofStr(waits_); };
        x.set = [this](const Value& v, std::string& err) {
            std::string w;
            for (char ch : v.s()) {
                const char u = (char)std::toupper((unsigned char)ch);
                if (u < 'A' || u > 'H') {
                    err = "J1 has positions A to H only, such as B or CG";
                    return false;
                }
                if (w.find(u) == std::string::npos) w += u;
            }
            waits_ = w;
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "clock_hz";
        x.help = "The 80286's clock: 8000000, 10000000 or 12500000 as sold. 0 runs flat out, timed as 8000000";
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
// SNAPSHOT / RESTORE: the 80286 and the memory manager. The sockets' image is a property
// (reloaded at power), not state.
// ---------------------------------------------------------------------------
void CompuProCpu286Board::serialize(StateWriter& w) const {
    Board::serialize(w);
    core_->serialize(w);
    w.u8(mm_);
}

void CompuProCpu286Board::deserialize(StateReader& r) {
    Board::deserialize(r);
    core_->deserialize(r);
    setMm(r.u8());
    publishPolicy();
}

} // namespace altair
