#pragma once

// CompuPro M-Drive/H (board 197B): 512K of dynamic RAM that the system uses as a disk drive.
// reference/CompuPro M-Drive H.md; docs/boards/compupro-mdriveh.md.
//
// TWO I/O PORTS, NO MEMORY. base + 0 = data (read/write), base + 1 = address (write).
// An OUT to the address port shifts a 22-bit counter up eight bits and puts the byte at the
// bottom (the 74LS161 chain, schematic p. 17): three OUTs, high byte first, load an address.
// Each byte through the data port moves the counter up by one.
//
// UP TO EIGHT BOARDS SHARE THE SAME TWO PORTS. Every board loads and counts; MA21-MA19
// against the board's S1-8..10 (`cascade`) pick the one that stores a write and drives a
// read. An absent board's range reads the floating bus, FF, and CompuPro's sizing routine
// depends on that. So the board shares both ports by design (Board::sharesPort) and drives
// the read lines only while selected (Board::drivesBits).

#include "core/board.h"

#include <cstdint>
#include <string>
#include <vector>

namespace altair {

class CompuProMDriveHBoard : public Board {
public:
    static constexpr uint32_t kSize    = 512 * 1024;    // one board
    static constexpr uint32_t kCounter = 1u << 22;      // MA21-MA0: eight boards

    CompuProMDriveHBoard();

    std::string type() const override { return "mdriveh"; }

    bool    decodes(const BusCycle& c) const override;
    bool    sharesPort(const BusCycle& c) const override;
    uint8_t drivesBits(const BusCycle& c) const override;
    uint8_t read(const BusCycle& c) override;
    void    write(const BusCycle& c) override;

    void reset(Reset) override;
    void power() override;

    void serialize(StateWriter& w) const override;
    void deserialize(StateReader& r) override;

    std::vector<Property> properties() override;
    std::vector<MapEntry> ioMap() const override;
    std::vector<std::string> statusLines() const override;

    // Test access.
    uint32_t counter() const { return counter_; }
    bool     selected() const { return ((counter_ >> 19) & 7) == cascade_; }
    uint8_t  byteAt(uint32_t offset) const { return ram_[offset % kSize]; }

private:
    enum class Fill { Zero, Random };

    bool isData(const BusCycle& c) const { return c.port() == base_; }
    bool isAddress(const BusCycle& c) const { return c.port() == (uint8_t)(base_ + 1); }
    void count() { counter_ = (counter_ + 1) & (kCounter - 1); }
    void fillRam();
    void refillIfNew();

    std::vector<uint8_t> ram_;
    uint32_t counter_ = 0;      // MA21-MA0
    uint8_t  base_    = 0xC6;   // S1-1..7 = A7-A1; CompuPro standard C6H/C7H
    uint8_t  cascade_ = 0;      // S1-8..10 = MA21-MA19 (ON = 0): the 1st board is 0
    Fill     fill_    = Fill::Random;
    uint64_t seed_    = 1;
    bool     new_     = true;   // just plugged in: no power-up, no cycle, no restore yet
};

} // namespace altair
