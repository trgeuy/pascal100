#pragma once
// THE COMPUPRO CPU 286 (board 451B, Viasyn 1986, doc 8261-0018B; Documentation/CompuPro/
// "CompuPro CPU 286 (v451B) Manual.pdf" in the project folder). An 80286 in real mode on a
// 16-bit IEEE 696 bus.
//
//   * THE 80286 (cpu/cpu80286.h): starts at FFFFF0 with A20-A23 high until the first far
//     jump. 16-bit transfers with SIXTN*; byte-serial, even byte first, otherwise.
//   * THE MEMORY MANAGER (p. 13): a 4-bit latch, written by an OUT of a byte to any EVEN port
//     01F8-01FE: D0-D3 (the low nibble of AL) = A20-A23. Reset to 0. A read in the range does
//     not reach the S-100 bus and leaves the latch alone. "Writing a byte or word to an odd
//     address in this range puts random data into the memory manager": here an odd port
//     leaves the latch unchanged (a word OUT to an even port runs as two byte cycles, the
//     second to the odd port, and must not spoil the first).
//   * J3-J6 (p. 7): per line, A-C (as shipped) = the memory manager drives it, B-C = the
//     80286 does. J3 = A23 ... J6 = A20. Property mm_lines, bit 0 = A20 (J6). NOTE: with
//     A-C the latch is 0 at reset, so the first fetch is at 0FFFF0, where the Disk 1A ROM
//     answers under PHANTOM* (p. 12); the sockets in page FF are reached at reset only with
//     the lines at B-C.
//   * THE CARD'S OWN PORTS (J2, p. 7): with J2 removed (the standard), the card decodes 00F8-
//     00FF (the 80287) and 01F8-01FF (the memory manager), all of A0-A15, and turns its S-100
//     data buffers off for them: the cycle runs on the bus, the core takes the local byte
//     (core/bus.h ownsIo). No 80287 is fitted here, so its ports read FF. J2 fitted gives all
//     sixteen ports back to the bus, and the memory manager can no longer be written.
//   * THE SOCKETS U37/U38 (J7, pp. 7-8 and 16): 64K in page FF (FF0000-FFFFFF), 16 bits
//     wide, on the card's side of the buffers (x86local.h: no S-100 cycle). J7 removed (as
//     shipped) = page FF is memory on the S-100 bus. Here they hold an EPROM pair: `rom` is
//     the 16-bit image (the even/odd split is the programmer's business), 16K (2764 pair),
//     32K (27128) or 64K (27256), repeated through the window. Writes are lost.
//   * WAIT STATES (J1, p. 6): A 1 memory, B 1 I/O, C 1 all, D 2 memory, E 2 I/O, F 2 all,
//     G 4 sockets, H 1 memory write + 1 I/O; additive. Property `waits`, letters A-H.
//   * NO INTERRUPT CONTROLLER on the card: INT* (pin 73) and NMI* reach the 80286; the SS-1's
//     8259As do the work (the SS-1 needs ECO 179, p. 4).
//   * Not modelled: the 80287 (J2 removed: its ports float), protected mode, SRAM/EEPROM in
//     the sockets (J8, J9), CDSB* (pin 19).
#include "core/board.h"
#include "core/bus.h"
#include "cpu/cpu.h"
#include "cpu/cpu80286.h"

#include <memory>
#include <string>
#include <vector>

namespace altair {

class CompuProCpu286Board : public Board, public BusMaster, public CpuCard {
public:
    // The slowest crystal it was sold with (8, 10 or 12.5 MHz): what a board divides by when
    // clock_hz is 0 (Clock::setHz).
    static constexpr long long kNominalHz = 8000000;

    CompuProCpu286Board();

    std::string type() const override { return "cpu286"; }

    // ---- BusMaster ----
    StepResult step(Bus& bus) override;

    // ---- CpuCard ----
    CpuCore* activeCore() override { return core_.get(); }
    std::vector<CoreUnit> cores() override { return {{"80286", core_.get()}}; }
    size_t coreCount() const override { return 1; }
    uint8_t peekCodeByte(CpuCore* core, const Bus& bus, uint32_t addr) override;
    void reportAchievedHz(long long hz) override { achievedHz_ = hz; }
    long long achievedHz() const override { return achievedHz_; }

    // ---- Board: nothing on the S-100 bus decodes to this card ----
    void reset(Reset r) override;
    void power() override;
    void clockAttached() override { publishPolicy(); }
    std::vector<std::string> drainLog() override {
        std::vector<std::string> out = std::move(log_);
        log_.clear();
        return out;
    }

    std::vector<Property> properties() override;
    std::vector<UnitDef> units() const override;
    std::vector<MapEntry> ioMap() const override;

    void serialize(StateWriter& w) const override;
    void deserialize(StateReader& r) override;

    // ---- For tests and SHOW ----
    Cpu80286* i80286() { return core_.get(); }
    uint8_t memoryManager() const { return mm_; }
    // The physical address of a linear one: A20-A23 from the memory manager where J3-J6 say.
    uint32_t physical(uint32_t linear) const;

private:
    // The 80286's hand on its own cycles, set around step(): I/O through core/bus.h
    // (AddressTranslator), memory through cpu/x86local.h.
    struct Side : AddressTranslator, X86LocalMemory {
        CompuProCpu286Board* card;
        explicit Side(CompuProCpu286Board* c) : card(c) {}
        // AddressTranslator
        uint32_t translate(uint16_t logical, Cycle) override { return logical; }
        uint16_t ioAddress(uint16_t addr, Cycle) override;
        bool ownsIo(uint16_t addr, Cycle t) const override;
        uint8_t localRead(uint16_t addr) override;
        void localWrite(uint16_t addr, uint8_t v) override;
        // X86LocalMemory
        bool cycle(uint32_t phys, bool write) override;
        bool owns(uint32_t phys) const override;
        uint8_t read(uint32_t phys) override { return peek(phys); }
        void write(uint32_t, uint8_t) override {}   // an EPROM: the byte is gone
        uint8_t peek(uint32_t phys) const override;
    };

    bool isOwnPort(uint16_t addr) const;
    bool inSockets(uint32_t phys) const;
    bool loadRom(const std::string& spec, std::string& err);
    void setMm(uint8_t v);
    void publishPolicy();
    bool wait(char j) const { return waits_.find(j) != std::string::npos; }

    std::unique_ptr<Cpu80286> core_;
    Side side_{this};

    uint8_t mm_ = 0;                // the memory manager latch: bit 0 = A20
    std::vector<uint8_t> rom_;      // the sockets' image (16K, 32K or 64K); empty = none
    std::vector<std::string> log_;  // a `rom` that would not load at power

    // The jumpers. Defaults: as shipped (p. 5), except J1 (not stated): B, one I/O wait.
    uint8_t mmLines_ = 0x0F;        // J3-J6: A-C on all four lines
    bool j2_ = false;               // J2 removed: the card's own ports
    bool sockets_ = false;          // J7 removed
    std::string romSpec_;
    std::string waits_ = "B";       // J1
    long long hz_ = 0;              // the CPU clock; 0 = flat out
    bool idle_ = true;
    long long achievedHz_ = 0;
};

} // namespace altair
