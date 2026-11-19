#pragma once
// THE COMPUPRO CPU 86/87 (board 186 rev B; reference/CompuPro CPU 86-87.md). An 8086 on a
// 16-bit IEEE 696 bus, with an 8087 socket (empty here: J8 fitted) and a ROM-less 80130.
//
//   * THE 8086 is the 8088 core with its 16-bit data bus on (Cpu8088::setWideBus): a word
//     at an even address asks for 16 bits (sXTRQ*), and with no SIXTN* the card runs the
//     byte-serial pair itself, the even byte first (schematic 5/9).
//   * THE CARD'S OWN PORTS are on the 8086's local bus, and they decode A8-A15, which no
//     bus decode may (board.h). So they are not bus decodes: the card's AddressTranslator
//     claims them (ownsIo), the cycle still runs on the S-100 bus, and the core takes the
//     local byte (core/bus.h).
//       - The MEMORY MANAGER: OUT to 00FD (S4-3..10 = A15-A8, low byte FD hard-wired).
//         D7-D4 of the byte = A23-A20 from the next cycle (p. 9). Cleared by RESET.
//       - The 80130 at FFF0-FFFF (S2 + S4-1,2), on with S3-2: its 8259A-like interrupt
//         controller (FFF0, FFF2) and three timers with hard-wired modes 2, 0, 3 (FFF8,
//         FFFA, FFFC, control FFFE), clocked by the CPU clock (pp. 11-18).
//   * THE 80130 REACHES NOTHING: jumpers J0-J7 are not fitted in the standard system (p. 8),
//     and they are not modelled. Its IR inputs read VI0*-VI7*; its INT and timer outputs go
//     nowhere, and it never answers an interrupt acknowledge.
//   * POWER-ON JUMP (S5): at power-on, and at RESET with S5-9, the 8086 starts at
//     ss00:0000 (the jammed JMPF, schematic 6/9) instead of FFFF0.
//   * WAIT STATES (S3-3, S3-4): one on every I/O cycle (the standard). One on every cycle
//     (S3-4 OFF) is not offered.
//   * Not modeled: the 8087, the 80130's ROM (S1, S3-1), MWRT (S3-7), PHI-DSB* (S3-8),
//     the separate 2 MHz CLOCK crystal (the bus models pin 49 itself, core/busclock.h).
#include "chips/intel8253.h"
#include "chips/intel8259a.h"
#include "core/board.h"
#include "core/bus.h"
#include "cpu/cpu.h"
#include "cpu/cpu8088.h"

#include <memory>

namespace altair {

class CompuProCpu8687Board : public Board, public BusMaster, public CpuCard {
public:
    CompuProCpu8687Board();

    std::string type() const override { return "cpu8687"; }

    // ---- BusMaster ----
    StepResult step(Bus& bus) override;

    // ---- CpuCard ----
    CpuCore* activeCore() override { return core_.get(); }
    std::vector<CoreUnit> cores() override { return {{"8086", core_.get()}}; }
    size_t coreCount() const override { return 1; }
    uint8_t peekCodeByte(CpuCore* core, const Bus& bus, uint32_t addr) override;
    void reportAchievedHz(long long hz) override { achievedHz_ = hz; }
    long long achievedHz() const override { return achievedHz_; }

    // ---- Board: nothing on the S-100 bus decodes to this card ----
    void reset(Reset r) override;
    void power() override;
    void clockAttached() override { publishPolicy(); }

    std::vector<Property> properties() override;
    std::vector<UnitDef> units() const override;
    std::vector<MapEntry> ioMap() const override;

    void serialize(StateWriter& w) const override;
    void deserialize(StateReader& r) override;

    // ---- For tests and SHOW ----
    Cpu8088* i8086() { return core_.get(); }
    uint8_t memoryManager() const { return mm_; }
    const Intel8259a& pic() const { return pic_; }
    std::string describe80130() const;

private:
    // The 8086's hand on its own cycles (core/bus.h AddressTranslator), set around step().
    struct Side : AddressTranslator {
        CompuProCpu8687Board* card;
        explicit Side(CompuProCpu8687Board* c) : card(c) {}
        uint32_t translate(uint16_t logical, Cycle) override { return logical; }
        uint16_t ioAddress(uint16_t addr, Cycle) override;
        bool ownsIo(uint16_t addr, Cycle t) const override;
        uint8_t localRead(uint16_t addr) override;
        void localWrite(uint16_t addr, uint8_t v) override;
    };

    bool isMm(uint16_t addr) const { return addr == mmPort_; }
    bool is80130(uint16_t addr) const;
    uint64_t timerNow() const;
    void setMm(uint8_t v);
    void publishPolicy();

    std::unique_ptr<Cpu8088> core_;
    Side side_{this};
    Intel8259a pic_{"80130"};
    Intel8253 pit_{"80130"};

    uint8_t mm_ = 0;   // U46: D7-D4 of the last OUT = A23-A20

    // The switches. Defaults: the "up and running in under 5 minutes" settings (p. 1).
    uint16_t mmPort_ = 0x00FD;     // S4-3..10 = A15-A8; A7-A0 = FD
    uint16_t port80130_ = 0xFFF0;  // S4-1,2 + S2 = A15-A4
    bool io80130_ = true;          // S3-2
    bool ioWait_ = true;           // S3-3 + S3-4: one wait state per I/O cycle
    bool poj_ = false;             // S5-10
    bool jor_ = false;             // S5-9
    uint32_t pojAddr_ = 0xFF000;   // S5-1..8 = A19-A12 (all OFF = 1)
    long long hz_ = 0;             // the CPU clock; 0 = flat out
    bool idle_ = true;
    long long achievedHz_ = 0;
};

} // namespace altair
