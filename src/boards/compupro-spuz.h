#pragma once
//
// CompuPro (Viasyn) SPUZ -- a slave processor board: a Z80H at 8 MHz, 64K or 256K of RAM and a
// Signetics 2681 DUART (board 257E). See reference/CompuPro SPUZ.md, DESIGN.md 3.0.4 and
// docs/boards/compupro-spuz.md.
//
// NOT A BUS MASTER (manual p. 8: "It does not become a temporary master"). The Z80 runs on the
// board's own bus. On the S-100 bus the board is a 64K memory window (the page set by SW2,
// CompuPro standard 0E0000h) and 18 ports at FC30h-FC41h, decoded on all of A0-A15 (sheet 2:
// U36 decodes A8-A15 = FCh, PAL U25 the rest). Up to 16 boards share the window and the ports.
// A write to SLAVESEL (FC40h) reaches every board, and only the board whose SW1 select matches
// D3-D0 keeps VPROC; only that board then answers the window, the 2681 at FC30h-FC3Fh and
// PTLATCH (FC41h).
//
// THE PRIVATE MACHINE. A CpuZ80, a Bus and a Clock of the board's own, as on the Jade Double D.
// The private bus has two boards: the RAM (the bank the 2681's OP5/OP4 select) and the 2681,
// which fills the Z80's I/O space (16 registers, repeated). The private clock counts board
// T-states at 8 MHz. The 2681 times itself on it, with its own 3.6864 MHz crystal.
//
// WHEN THE BOARD Z80 RUNS. Lazily, as on the Jade DD (catchUp/runTo): a host cycle the board
// decodes first runs the board Z80 up to the host's present. While the Z80 is out of reset, a
// host deadline also catches it up once each 1 ms of host time, so that a change on the VI line
// (2681 OP6) reaches the host without the host asking.
//
// THE BUS HANG. A host access to the window or to FC30h-FC3Fh while the Z80 is in reset hangs a
// real S-100 bus (manual pp. 9, 29: the board waits for a BUSACK* that a reset Z80 never gives).
// Here the cycle floats, the board logs the cycle and the PC, and the run stops at the
// instruction boundary (StopReason::BusHang).

#include "chips/scn2681.h"
#include "core/board.h"
#include "core/bus.h"
#include "core/clock.h"
#include "core/privatecpu.h"
#include "cpu/cpuZ80.h"

#include <cstdint>
#include <string>
#include <vector>

namespace altair {

class SpuzBoard : public Board {
public:
    SpuzBoard();
    ~SpuzBoard() override;

    std::string type() const override { return "spuz"; }

    // The monitor installs it; the board turns a CONNECT endpoint into a stream with it.
    static void setResolver(EndpointResolver r);

    // ---- the host side: the window and the ports ----
    bool    decodes(const BusCycle& c) const override;
    bool    decodesIoHigh(uint8_t port) const override { return port >= 0x30 && port <= 0x41; }
    bool    decodesExtended() const override { return true; }
    bool    sharesPort(const BusCycle& c) const override;
    uint8_t drivesBits(const BusCycle& c) const override;
    bool    assertsPhantom(const BusCycle& c) const override;
    uint8_t read(const BusCycle& c) override;
    void    write(const BusCycle& c) override;
    bool    peek(uint16_t addr, uint8_t& out) const override;
    bool    peekX(uint32_t phys, uint8_t& out) const override;
    uint8_t assertsVi() const override { return viPulled_ ? viBit(irq_) : 0; }
    bool    backplaneConflict(const std::vector<Board*>& all, std::string& why) const override;

    void reset(Reset) override;
    void power() override;
    void configChanged() override;
    void clockAttached() override;

    std::vector<Property>    properties() override;
    std::vector<std::string> statusLines() const override;
    std::vector<MapEntry>    ioMap() const override;
    std::vector<MapEntry>    memMap() const override;

    // ---- the two RS-232 channels (2681 channels A and B) ----
    std::vector<UnitDef>  units() const override;
    std::vector<Property> boardUnitProperties(const std::string& unit) override;
    bool connect(const std::string& unit, const std::string& endpoint, std::string& err) override;
    bool connectStream(const std::string& unit, std::unique_ptr<ByteStream> s,
                       std::string& err) override;
    bool disconnect(const std::string& unit, std::string& err) override;
    ByteStream* unitStream(const std::string& unit) override;
    void        pump() override;
    uint64_t    rxBytes() const override { return duart_.rxBytes(0) + duart_.rxBytes(1); }
    // NO emptyPolls(). The idle judgement compares empty status reads with the MAIN
    // processor's instructions (guestIsWaiting). This board's own Z80 polls its 2681 while
    // the main processor computes, and those reads would pass for the main processor waiting.

    std::vector<std::string> drainLog() override;

    // The board Z80 for the debugger: "spuz0:z80" (DESIGN.md 3.0.4, stage 4).
    std::vector<PrivateCpu*> privateCpus() override { return {&z80Unit_}; }

    void serialize(StateWriter& w) const override;
    void deserialize(StateReader& r) override;

    // For tests, without going through the bus.
    CpuZ80&  z80() { return cpu_; }
    Scn2681& duart() { return duart_; }
    Clock&   boardClock() { return bclk_; }
    bool     selected() const { return selected_; }
    bool     inReset() const { return (ptlatch_ & kZrst) == 0; }
    uint8_t  ptlatch() const { return ptlatch_; }
    int      z80Bank() const { return zbank_; }
    uint8_t  ram(uint32_t a) const { return ram_[a % kRamMax]; }
    void     catchUp();
    PrivateCpu& z80Unit() { return z80Unit_; }

    static constexpr long long kBoardHz = 8000000;  // X1 16 MHz / 2 (manual p. 8)
    static constexpr uint32_t  kRamMax  = 0x40000;  // 256K

private:
    // ---- the private bus's two boards ----

    // Eight 4164s or eight 41256s. A 256K board shows the Z80 the bank OP5/OP4 select. The DRAM
    // needs an M1 wait state above 5 MHz (S1-4, manual p. 8).
    struct Ram : Board {
        SpuzBoard* d = nullptr;
        std::string type() const override { return "spuz-ram"; }
        std::vector<Property> properties() override { return {}; }
        bool decodes(const BusCycle& c) const override {
            return c.type == Cycle::MemRead || c.type == Cycle::MemWrite;
        }
        uint8_t read(const BusCycle& c) override {
            if (d->m1Wait_ && (c.status & StM1)) holdReady(1);
            return d->ram_[d->zOffset() | c.addr];
        }
        void write(const BusCycle& c) override { d->ram_[d->zOffset() | c.addr] = c.data; }
        bool peek(uint16_t a, uint8_t& out) const override {
            out = d->ram_[d->zOffset() | a];
            return true;
        }
    };

    // The 2681 on the Z80's I/O space: CEN* = IORQ*, A3-A0 = ZA3-ZA0 (sheet 4), so it answers
    // every port, 16 registers repeated (p. 8). Its INTRN is the Z80's INT* (ECO 138: OP7 off).
    struct LocalIo : Board {
        SpuzBoard* d = nullptr;
        std::string type() const override { return "spuz-io"; }
        std::vector<Property> properties() override { return {}; }
        bool decodes(const BusCycle& c) const override {
            return c.type == Cycle::IoRead || c.type == Cycle::IoWrite;
        }
        uint8_t drivesBits(const BusCycle& c) const override { return regDriven(c.port()); }
        uint8_t read(const BusCycle& c) override { return d->duartRead(c.port() & 0x0F); }
        void    write(const BusCycle& c) override { d->duartWrite(c.port() & 0x0F, c.data); }
        bool    assertsInt() const override { return d->zint_; }
    };

    // The board Z80 as the debugger reaches it. Its code comes from the bank OP5/OP4 select;
    // a step by hand runs the Z80 and not the host (stepOne).
    struct Z80Unit : PrivateCpu {
        SpuzBoard* d = nullptr;
        Z80Unit() : PrivateCpu("z80") {}
        CpuCore& core() override { return d->cpu_; }
        uint8_t  peekCode(uint32_t a) const override { return d->ram_[d->zOffset() | (a & 0xFFFF)]; }
        bool     step(std::string& err) override {
            if (!d->stepOne(err)) return false;
            stepped();
            return true;
        }
    };

    // PTLATCH (FC41h, U28, a 74LS175 cleared by CLR*; manual p. 10, sheet 3).
    static constexpr uint8_t kZrst = 0x08;  // 1: the Z80 and the 2681 run; 0: reset
    static constexpr uint8_t kVdma = 0x04;  // 1: this board is the DMA board (not modeled)
    static constexpr uint8_t kPb   = 0x03;  // the 64K bank the host sees

    // The data lines a 2681 read drives: none for 2, A, C, E and F, whose data no sheet gives
    // (chips/scn2681.h). The bus then floats them (DESIGN.md 4.6.1).
    static uint8_t regDriven(uint8_t port) {
        switch (port & 0x0F) {
            case 0x2: case 0xA: case 0xC: case 0xE: case 0xF: return 0x00;
            default: return 0xFF;
        }
    }

    bool windowHit(uint16_t, uint8_t ext) const { return selected_ && ext == window_; }
    uint32_t hostOffset() const { return big_ ? (uint32_t)(ptlatch_ & kPb) << 16 : 0; }
    uint32_t zOffset() const { return (uint32_t)zbank_ << 16; }

    uint8_t duartRead(int reg);            // the Z80's or the host's read of a register
    void    duartWrite(int reg, uint8_t v);
    void    duartRefresh();                // poll the 2681, follow its pins, re-arm its event
    void    followPins();                  // OP6 -> VI, OP5/OP4 -> the Z80 bank, INTRN -> INT*
    void    writeSlaveSel(uint8_t v);
    void    writePtlatch(uint8_t v);
    void    enterReset();
    void    hang(const BusCycle& c);       // the access a real SPUZ in reset never finishes

    void runTo(uint64_t target);
    bool stepOne(std::string& err);        // the debugger's STEP spuz0:z80
    void rearmHost();
    void hostWake();
    Clock& hostClk() const;

    // ---- the private machine ----
    Clock   bclk_;
    Bus     bbus_;
    CpuZ80  cpu_;
    Ram     ramBoard_;
    LocalIo ioBoard_;
    Scn2681 duart_{"duart"};
    Z80Unit z80Unit_;
    std::vector<uint8_t> ram_;
    Clock::Handle duartWake_ = Clock::kNone;

    // ---- the latches ----
    bool    selected_ = false;  // VPROC (U17), cleared by CLR*
    uint8_t ptlatch_  = 0;      // U28, cleared by CLR*: the Z80 starts in reset
    // Followed from the 2681's pins (followPins).
    bool    zint_     = false;  // INTRN: the Z80's INT*
    bool    viPulled_ = true;   // OP6 high: Q3 pulls the J2 VI line
    int     zbank_    = 3;      // OP5/OP4: the Z80 starts in bank 3 (p. 12)

    // ---- the host-to-board time conversion (catchUp), as on the Jade DD ----
    uint64_t  anchorH_ = 0;
    uint64_t  rem_     = 0;
    long long lastHz_  = 0;
    uint64_t  targetB_ = 0;
    bool      inCatchUp_ = false;
    Clock::Handle hostWake_ = Clock::kNone;
    uint64_t  lastHangAt_ = UINT64_MAX;  // one hang message per host instruction

    // ---- the switches and jumpers ----
    uint8_t   select_ = 0;      // SW1 5-8: the board select, 0-15
    uint8_t   window_ = 0x0E;   // SW2: A23-A16 of the 64K window (CompuPro standard 0Eh)
    bool      big_    = true;   // 256K (eight 41256s) or 64K (eight 4164s)
    int       waits_  = 2;      // J1: 1, 2 or 3 S-100 wait states (B, 2, as shipped)
    bool      m1Wait_ = true;   // SW1 4: the Z80 M1 wait state (ON, as shipped)
    IrqJumper irq_    = IrqJumper::Vi0;  // J2: VI0 as shipped

    std::vector<std::string> log_;
};

} // namespace altair
