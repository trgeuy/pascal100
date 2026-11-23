#pragma once
// A 16-bit I/O board for the tests: two registers at an even port and the port after it,
// which answers SIXTN* on an I/O cycle (IEEE 696 2.6) when `sixteen` is on. The shape of
// a 16-bit peripheral such as a graphics controller's command FIFO. A harness board, not a
// model of any card. Used by test_ieee696.cpp, test_cpu8687.cpp and test_cpu286.cpp.
#include "core/board.h"

#include <string>
#include <vector>

namespace altair {

struct Io16Board : Board {
    uint8_t base = 0x70;            // the even port; port() & 0xFE == base decodes
    bool sixteen = true;
    uint8_t reg[2]{0x11, 0x22};     // [0] = the even port, [1] = the odd port
    int reads = 0, writes = 0, reads16 = 0, writes16 = 0;

    std::string type() const override { return "io16test"; }
    std::vector<Property> properties() override { return {}; }
    bool decodes(const BusCycle& c) const override {
        return (c.type == Cycle::IoRead || c.type == Cycle::IoWrite) &&
               (c.port() & 0xFE) == base;
    }
    uint8_t read(const BusCycle& c) override { ++reads; return reg[c.port() & 1]; }
    void write(const BusCycle& c) override { ++writes; reg[c.port() & 1] = c.data; }
    bool answersSixtn(const BusCycle& c) const override {
        return sixteen && (c.type == Cycle::IoRead || c.type == Cycle::IoWrite);
    }
    void read16(const BusCycle&, uint8_t& even, uint8_t& odd) override {
        ++reads16;
        even = reg[0];
        odd  = reg[1];
    }
    void write16(const BusCycle& c) override {
        ++writes16;
        reg[0] = c.data;
        reg[1] = c.dataHi;
    }
    void set16(bool on) { sixteen = on; decodeChanged(); }
};

} // namespace altair
