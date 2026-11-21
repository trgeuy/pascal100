#pragma once
// THE COMPUPRO CPU 8085/88 (161F; reference/compupro/cpu-8085-88.md). Two processors on one
// card, one on line at a time: an 8085A-2 and an 8088 (minimum mode). The other one is held
// in HOLD.
//
//   * ONE PORT, S3 (factory FD), decoded on A0-A7. An IN from it SWAPS the processors (manual
//     p.11: "a simple INPUT instruction from the Processor Swap Port") and reads FF. An OUT
//     latches the MEMORY MANAGER byte (U28).
//   * THE MEMORY MANAGER. With the 8085 on line the latch drives A16-A23. With the 8088 on
//     line the 8088 drives A0-A19 and the latch's HIGH nibble drives A20-A23 (p.11). The full
//     byte stays latched. Cleared at power-on; on RESET* only with XAC (S1-2).
//   * START-UP. The 8085 runs first. Power-on-jump (S1-8 POJ): the 8085's first three reads
//     get C3 00 <S2>, a JMP to the S2 page. The 8088's first time on line it starts from its
//     reset state, FFFF0 (with A20-A23 from the latch). After that each processor resumes
//     where it stopped, or is reset as it comes on line: 5RS (S1-4), 8RS (S1-5). A reset-on-
//     swap 8085 does the power-on-jump again only with JOR (S1-6) on as well: the schematic
//     arms U5b from POC*, or from the 8085's RESET OUT through S1-6 (sheet 5).
//   * IOW (S1-3): one wait state in every I/O cycle, of either processor.
//   * INTERRUPTS reach only the processor on line (p.15), which here falls out of stepping
//     only that core. The 8085 gets pINT* on INTR. NOT MODELED: NMI* to the 8085's TRAP and
//     VI0*-VI2* to its RST 5.5/6.5/7.5 -- the 8085 core does not wire those pins yet.
//   * THE SWAP itself is immediate: the IN's cycle changes the active core, and the run loop
//     runs the other processor from the next instruction boundary. The real board finishes
//     the current bus cycle, then trades HOLD/HLDA (pp.16-17); no instruction is cut in two.
//   * Not modeled: MWRITE (S1-7), the IMSAI front panel (J2, J6), XA3 (S1-1, ADSB*).
#include "core/board.h"
#include "core/bus.h"
#include "cpu/cpu.h"
#include "cpu/cpu8085.h"
#include "cpu/cpu8088.h"

#include <memory>

namespace altair {

class Cpu8588Board : public Board, public BusMaster, public CpuCard {
public:
    // What a board divides by when the running core's clock is 0 (Clock::setHz): the 8085 at
    // 6 MHz (S4 right, the manual's text and schematic) and the 8088 at 8 MHz.
    static constexpr long long kNominal85Hz = 6000000;
    static constexpr long long kNominal88Hz = 8000000;

    Cpu8588Board();

    std::string type() const override { return "cpu8588"; }

    // ---- BusMaster ----
    StepResult step(Bus& bus) override;

    // ---- CpuCard ----
    CpuCore* activeCore() override {
        return on88_ ? static_cast<CpuCore*>(c88_.get()) : static_cast<CpuCore*>(c85_.get());
    }
    std::vector<CoreUnit> cores() override { return {{"8085", c85_.get()}, {"8088", c88_.get()}}; }
    size_t coreCount() const override { return 2; }
    void settleSwitch() override;
    uint8_t peekCodeByte(CpuCore* core, const Bus& bus, uint32_t addr) override;
    void reportAchievedHz(long long hz) override { achievedHz_ = hz; }
    long long achievedHz() const override { return achievedHz_; }

    // ---- Board ----
    bool decodes(const BusCycle& c) const override;
    uint8_t read(const BusCycle& c) override;
    void write(const BusCycle& c) override;
    void reset(Reset r) override;
    void power() override;
    void clockAttached() override { publishPolicy(); }

    std::vector<Property> properties() override;
    std::vector<UnitDef> units() const override;
    std::vector<MapEntry> ioMap() const override;

    void serialize(StateWriter& w) const override;
    void deserialize(StateReader& r) override;

    Cpu8085* i8085() { return c85_.get(); }
    Cpu8088* i8088() { return c88_.get(); }
    bool on8088() const { return on88_; }
    uint8_t latch() const { return latch_; }

private:
    // The 8085's view of the bus: the Memory Manager in front of its 16-bit addresses, the
    // power-on-jump bytes on its first reads, and the IOW wait state.
    struct Side85 : AddressTranslator {
        Cpu8588Board* card;
        explicit Side85(Cpu8588Board* c) : card(c) {}
        uint32_t translate(uint16_t logical, Cycle) override {
            return ((uint32_t)card->latch_ << 16) | logical;
        }
        uint16_t ioAddress(uint16_t addr, Cycle) override;
        uint8_t readData(uint8_t v) override;
    };
    // The 8088's: it drives A0-A19 itself (memReadX, never translated); only the IOW wait
    // state needs the I/O hook.
    struct Side88 : AddressTranslator {
        Cpu8588Board* card;
        explicit Side88(Cpu8588Board* c) : card(c) {}
        uint32_t translate(uint16_t logical, Cycle) override { return logical; }
        uint16_t ioAddress(uint16_t addr, Cycle) override;
    };

    void swap();                 // PORTPULSE*: the other processor comes on line
    void setLatch(uint8_t v);    // U28, and the 8088's A20-A23 from its high nibble
    void publishPolicy();

    std::unique_ptr<Cpu8085> c85_;
    std::unique_ptr<Cpu8088> c88_;
    Side85 side85_{this};
    Side88 side88_{this};

    bool on88_ = false;          // 8/5*: 1 = the 8088 is on line
    uint8_t latch_ = 0;          // U28, the Memory Manager byte
    uint8_t jumpReads_ = 3;      // power-on-jump reads done (3 = none pending)
    bool publishedFor88_ = false;   // which core's clock the Clock has

    // The switches. Defaults: the manual's quick-start settings (p.4) and port FD.
    uint8_t port_ = 0xFD;        // S3
    bool xac_ = true;            // S1-2: RESET* clears the Memory Manager
    bool iow_ = true;            // S1-3: one wait state per I/O cycle
    bool swapReset85_ = false;   // S1-4 5RS
    bool swapReset88_ = false;   // S1-5 8RS
    bool jor_ = true;            // S1-6 JOR: power-on-jump on RESET* too
    bool poj_ = false;           // S1-8 POJ
    uint16_t pojAddr_ = 0;       // S2: A8-A15 of the jump
    bool reset88_ = true;        // J7 A-B (factory): RESET* resets the 8088 too
    long long hz85_ = 0;         // S4: 6 or 2 MHz; 0 = flat out
    long long hz88_ = 0;         // X2 / 3: 8 MHz; 0 = flat out
    bool idle_ = true;
    long long achievedHz_ = 0;
};

} // namespace altair
