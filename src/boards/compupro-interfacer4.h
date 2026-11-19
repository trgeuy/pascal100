#pragma once
//
// CompuPro Interfacer 4 (Godbout board 187C, manual 5/83) -- three 2651 serial channels, a
// Centronics printer port and a Universal Parallel port, behind the Interfacer 3's 8-port
// block. See docs/boards/compupro-interfacer4.md and Reference/compupro/interfacer-4.md (the
// manual, converted, with the logic diagram described).
//
//   * HALF A GROUP. A board holds four users: relative user 0 = the two parallel ports,
//     1-3 = the 2651s U36, U35, U34. S2-1/S2-2 pick the group of eight (D3-D4 of the user
//     select byte) and S2-3 the half (D2). Two boards, one per half, make a group, and an
//     Interfacer 3 can share the base (in another group).
//
//   * THE USER SELECT LATCH (U18) is the Interfacer 3's: every board at the base latches every
//     write to relative port 6 or 7 (A0 not decoded). Taken in snoop().
//
//   * PORTS 0-3 answer on the board whose group AND half match the latch. PORTS 4/5 answer on
//     BOTH boards of the matching group (USEL has no H/L term): each drives its own nibble
//     (S3, which the model wires from S2-3) -- Board::drivesBits -- and the bus floats the
//     nibble nobody drives to 1s. A mask write loads each board's nibble; it is taken in
//     snoop() so both boards of the group latch it, and decodes() claims it only on the half
//     the latch names, so the bus delivers it once.
//
//   * STATUS AND MASK BITS are wired to the chips: nibble bit 0 = the parallel ports (TxI4 =
//     the Centronics acknowledge flop, RxI4 = the Universal Parallel input flop), bits 1-3 =
//     U36, U35, U34. J26 swaps only the chip enables of relative users 0 and 2.
//
//   * THE CENTRONICS PORT: a data write goes to the unit's endpoint (a file, a print queue, a socket), and a short handshake
//     later the printer's ACKNLG* sets TxI4. A data write clears TxI4; reset does not.

#include "chips/sig2651.h"
#include "core/board.h"

#include <memory>
#include <string>
#include <vector>

namespace altair {

class InterfacerFourBoard : public Board {
public:
    InterfacerFourBoard();
    ~InterfacerFourBoard() override;

    std::string type() const override { return "interfacer4"; }

    // "CompuPro Interfacer 4 port 7 (if4)": the absolute user number, which CompuPro's
    // software calls the port (MP/M 8-16's TTYS). The two parallel units share relative
    // user 0's port and add their own name.
    std::string defaultUnitTitle(const std::string& unit) const override;

    bool    decodes(const BusCycle& c) const override;
    uint8_t drivesBits(const BusCycle& c) const override;
    uint8_t read(const BusCycle& c) override;
    void    write(const BusCycle& c) override;
    bool    wantsSnoop() const override { return true; }  // U18 and the mask latches
    void    snoop(const BusCycle& c) override;

    // JS5/JS6 reach VI0-VI7 only.
    bool    assertsInt() const override { return false; }
    uint8_t assertsVi() const override;

    void reset(Reset) override;
    void power() override;
    void pump() override;
    void configChanged() override;

    void serialize(StateWriter& w) const override;
    void deserialize(StateReader& r) override;

    uint64_t rxBytes() const override;
    // The guest found no byte on a status read, on any user, or on the RxRDY port with no
    // user ready (Board::emptyPolls, altairsim #948).
    uint64_t emptyPolls() const override;
    std::vector<std::string> drainLog() override;

    std::vector<Property> properties() override;
    std::vector<Property> boardUnitProperties(const std::string& unit) override;
    std::vector<UnitDef>  units() const override;
    std::vector<MapEntry> ioMap() const override;

    bool connect(const std::string& unit, const std::string& endpoint, std::string& err) override;
    bool connectStream(const std::string& unit, std::unique_ptr<ByteStream> s,
                       std::string& err) override;
    bool disconnect(const std::string& unit, std::string& err) override;
    ByteStream* unitStream(const std::string& unit) override;

    static void setResolver(EndpointResolver r);

    // Test access.
    uint8_t selectLatch() const { return latch_; }
    bool    selected() const { return groupMatch() && halfMatch(); }
    bool    printerInterrupt() const { return txi_; }

private:
    // A 2651 channel: relative user 1, 2 or 3.
    struct Serial {
        explicit Serial(const std::string& n) : uart(n) {}
        Sig2651   uart;
        IrqJumper txIrq = IrqJumper::None;  // JS5
        IrqJumper rxIrq = IrqJumper::None;  // JS6
    };
    // A parallel port's line: the Centronics output or the Universal Parallel port.
    struct Line {
        std::unique_ptr<ByteStream> stream;
        std::string                 spec = "null";  // as written: SHOW and CONFIG SAVE echo it
        IrqJumper                   irq = IrqJumper::None;
    };

    static constexpr int kSerial = 3;

    bool groupMatch() const { return ((latch_ >> 3) & 3) == group_; }
    bool halfMatch() const { return ((latch_ >> 2) & 1) == half_; }
    // S2-4 is the Board's own `enabled` property (enabled_): ON grounds ASEL.
    bool inBlock(uint16_t port) const { return enabled_ && (port & 0xF8) == base_; }
    // The chip behind relative user `rel` (0-3): 0 = the parallel ports, 1-3 = U36/U35/U34.
    // J26 at A-C/B-D swaps 0 and 2.
    int  chipFor(int rel) const { return swap_ && (rel == 0 || rel == 2) ? 2 - rel : rel; }
    int  selectedChip() const { return chipFor(latch_ & 3); }
    int  shift() const { return half_ ? 4 : 0; }  // S3: the nibble this board drives

    void setLatch(uint8_t v);
    int  serialIndex(const std::string& unit) const;  // 0-2 for user1-user3, -1 otherwise
    Line* lineFor(const std::string& unit);
    bool attach(Line& l, std::unique_ptr<ByteStream> s, const std::string& spec);
    void logUart(Serial& s);

    uint8_t txReadyBits() const;  // nibble: bit 0 TxI4, bits 1-3 TxRDY of U36/U35/U34
    uint8_t rxReadyBits() const;  // nibble: bit 0 RxI4, bits 1-3 RxRDY
    uint8_t statusC() const;

    void printerWrite(uint8_t v);
    void onAcknowledge();
    void takeParallelInput();

    void     refresh();
    uint64_t nextEdge() const;

    std::vector<std::unique_ptr<Serial>> serial_;  // stable addresses: properties capture them
    Line lpt_;  // the Centronics port (unit "centronics")
    Line par_;  // the Universal Parallel port (unit "parallel")

    uint8_t base_   = 0x10;  // S2-5..9 = A7-A3 (CompuPro standard 10H)
    int     group_  = 0;     // S2-1/S2-2: users 0-7, 8-15, 16-23 or 24-31
    int     half_   = 1;     // S2-3: 0 = the low four of the group, 1 = the high four (4-7)
    int     waits_  = 0;     // J6/J7/J8: 0-3 wait states on ports 0-3
    bool    swap_   = false; // J26: false = A-B/C-D (standard), true = A-C/B-D
    uint8_t sense_  = 0xFF;  // S1-3..10 read on D7..D0; OFF = 1

    uint8_t latch_   = 0;    // U18: D0-D4 of the last user select write
    uint64_t emptyPolls_ = 0;  // RxRDY port and parallel status reads that found nothing (host bookkeeping)
    uint8_t txMask_  = 0;    // U48: TxINTEN, nibble bit 0-3
    uint8_t rxMask_  = 0;    // U52: RxINTEN
    uint8_t control_ = 0;    // U13: Control-C D0-D3 (AFX, INIT, SLCTIN, LED)

    bool     txi_      = false;  // U16b: the Centronics acknowledge flop (TxI4)
    uint64_t ackLowAt_ = 0;      // the printer's ACKNLG* pulse: low from here...
    uint64_t ackEndAt_ = 0;      // ...to here, BUSY until here; 0 = no handshake in flight
    Clock::Handle ack_ = Clock::kNone;

    bool    dav_   = false;  // U8a: Universal Parallel input latched (RxI4, Status-P D0)
    bool    dntkn_ = false;  // U8b: output written, not yet taken (Status-P D1)
    uint8_t inLatch_ = 0xFF; // U6

    Clock::Handle            wake_ = Clock::kNone;
    std::vector<std::string> log_;
};

} // namespace altair
