#include "boards/compupro-mdriveh.h"

#include "core/statefile.h"

#include <algorithm>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace altair {

CompuProMDriveHBoard::CompuProMDriveHBoard() : ram_(kSize, 0) {}

// ---------------------------------------------------------------------------
// Decode (U18 port compare on A7-A1, U7/U6 cycle decode)
// ---------------------------------------------------------------------------
// The data port takes reads and writes; the address port takes writes only. No routine in
// the manual reads the address port, and the PAL (197P-1) equations are not published, so
// a read there is not decoded: it floats to FF (reference §4, inferred).
bool CompuProMDriveHBoard::decodes(const BusCycle& c) const {
    if (!enabled_) return false;
    if (c.type == Cycle::IoRead) return isData(c);
    if (c.type == Cycle::IoWrite) return isData(c) || isAddress(c);
    return false;
}

// Every M-Drive/H in the system sits at the same two ports, by design.
bool CompuProMDriveHBoard::sharesPort(const BusCycle& c) const { return decodes(c); }

// Only the selected board puts a byte on DI0-DI7 (OE*). The others drive nothing, so a read
// of a range with no board floats to FF -- the bus's answer, never this board's.
uint8_t CompuProMDriveHBoard::drivesBits(const BusCycle&) const { return selected() ? 0xFF : 0x00; }

// ---------------------------------------------------------------------------
// The cycles. Every board loads and counts, selected or not, so the counters of all the
// boards stay equal (reference §4: inferred from the design, the PAL is not published).
// ---------------------------------------------------------------------------
uint8_t CompuProMDriveHBoard::read(const BusCycle& c) {
    if (!isData(c)) return 0;  // not decoded; the bus never asks
    const uint8_t v = ram_[counter_ % kSize];  // masked off by drivesBits() if not selected
    count();
    return v;
}

void CompuProMDriveHBoard::write(const BusCycle& c) {
    if (isAddress(c)) {
        // LOAD*: each counter loads the outputs eight bits below; U15/U16 load the data bus.
        counter_ = ((counter_ << 8) | c.data) & (kCounter - 1);
        return;
    }
    if (isData(c)) {
        if (selected()) ram_[counter_ % kSize] = c.data;
        count();
    }
}

// ---------------------------------------------------------------------------
// Reset and power. RESET touches neither the RAM nor the counter: the 74LS161 clear inputs
// are tied high (schematic p. 17). At power-up the RAM holds whatever the DRAM holds
// (`fill`); the counter's power-up state is not defined, and software always loads it
// before use, so it starts at 0 here.
// ---------------------------------------------------------------------------
void CompuProMDriveHBoard::reset(Reset) {}

void CompuProMDriveHBoard::power() {
    fillRam();
    counter_ = 0;
}

// 0xFF does not appear here on purpose: it is the bus's answer for an empty slot, and a
// board that seeds its store with it looks like a board that is not there (DESIGN.md 4.6.1).
void CompuProMDriveHBoard::fillRam() {
    if (fill_ == Fill::Zero) {
        std::fill(ram_.begin(), ram_.end(), 0);
        return;
    }
    std::mt19937_64 rng(seed_);
    for (auto& b : ram_) b = (uint8_t)(rng() & 0xFF);
}

void CompuProMDriveHBoard::serialize(StateWriter& w) const {
    Board::serialize(w);
    w.u32(counter_);
    w.blob(ram_);
}

void CompuProMDriveHBoard::deserialize(StateReader& r) {
    Board::deserialize(r);
    counter_ = r.u32() & (kCounter - 1);
    std::vector<uint8_t> all = r.blob();
    const size_t n = std::min(all.size(), ram_.size());
    std::copy(all.begin(), all.begin() + (std::ptrdiff_t)n, ram_.begin());
}

// ---------------------------------------------------------------------------
// Properties: the S1 switch and the power-up contents.
// ---------------------------------------------------------------------------
std::vector<Property> CompuProMDriveHBoard::properties() {
    std::vector<Property> p;
    {
        Property x;
        x.name  = "port";
        x.help  = "Switch S1-1..7 (A7-A1): the data port; the address port is the next one. "
                  "CompuPro software uses C6H";
        x.kind  = Kind::Int;
        x.radix = 16;
        x.min   = 0;
        x.max   = 0xFE;
        x.get   = [this] { return Value::ofInt(base_); };
        x.set   = [this](const Value& v, std::string& err) {
            if (v.i() & 1) {
                err = "the M-Drive/H compares A7-A1 and A0 picks data or address -- the port "
                      "must be even";
                return false;
            }
            base_ = (uint8_t)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "cascade";
        x.help = "Switch S1-8..10: which 512K of the 4M address range this board holds, 0-7. "
                 "The 1st board is 0; every board in the system needs its own";
        x.kind = Kind::Int;
        x.min  = 0;
        x.max  = 7;
        x.get  = [this] { return Value::ofInt(cascade_); };
        x.set  = [this](const Value& v, std::string&) {
            cascade_ = (uint8_t)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name    = "fill";
        x.help    = "RAM contents at power-on: zero | random (real DRAM is not zeroed)";
        x.kind    = Kind::Enum;
        x.choices = {"zero", "random"};
        x.get     = [this] { return Value::ofStr(fill_ == Fill::Zero ? "zero" : "random"); };
        x.set     = [this](const Value& v, std::string&) {
            fill_ = (v.s() == "zero") ? Fill::Zero : Fill::Random;
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "seed";
        x.help = "Seed for fill=random. The same seed fills the RAM the same way at every "
                 "POWER, so a run is repeatable";
        x.kind = Kind::Int;
        x.get  = [this] { return Value::ofInt((long long)seed_); };
        x.set  = [this](const Value& v, std::string&) {
            seed_ = (uint64_t)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    return p;
}

std::vector<MapEntry> CompuProMDriveHBoard::ioMap() const {
    if (!enabled_) return {};
    const uint32_t b = base_;
    return {
        {b + 0, b + 0, "read/write", "data: the byte at the counter, then the counter + 1"},
        {b + 1, b + 1, "write", "address: shift the counter up 8 bits, load the byte"},
    };
}

} // namespace altair
