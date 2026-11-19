#pragma once
//
// CompuPro Interfacer 3 (Godbout/Viasyn board 174, rev C) -- eight serial channels on eight
// 2651 USARTs behind one 8-port block. See docs/boards/compupro-interfacer3.md and
// Reference/compupro/interfacer-3.md (the manual, converted).
//
//   * ONE 8-PORT BLOCK FOR EIGHT USARTS. S1-2..6 compare A7-A3; S1-1 ON takes the board off
//     the bus. Relative ports 0-3 are the SELECTED USART's data, status (W: SYN/DLE), mode
//     and command registers. 4 and 5 read TxRDY0-7 and RxRDY0-7 and write the two interrupt
//     masks. 6 and 7 (A1 = A2 = 1, A0 not decoded) write the user select latch U34; an IN
//     there is not decoded.
//
//   * THE USER SELECT LATCH. D0-D2 pick the relative user (the USART), D3-D4 the board.
//     EVERY Interfacer 3 at the same base latches EVERY user select write -- the latch is
//     clocked by the address compare alone. A board then answers ports 0-5 only while D3-D4
//     match its S1-7/S1-8 (USEL). So up to four boards share one block and give 32 users.
//     Here the latch is taken in snoop(), so a board latches the write whether or not it is
//     the one the bus delivered it to, and decodes() follows USEL (decodeChanged() when it
//     moves). The select write itself is decoded by the board that is selected when it
//     arrives, so exactly one board claims it.
//
//   * INTERRUPTS. TxINTk = TxRDYk AND TxINTENk, RxINTk = RxRDYk AND RxINTENk (74LS38
//     open-collector NANDs), each jumpered on J15/J16 to any VI line. Reset clears both mask
//     latches. The status ports read the RDY lines whatever the masks. No pin-73 position.
//
// The USART is the existing Sig2651 (src/chips/sig2651.h), as on the System Support 1 -- A
// CHIP IS NOT A CARD (DESIGN.md 7.8). The board owns the decode, the latch, the masks, the
// interrupt gates and the wait states.

#include "chips/sig2651.h"
#include "core/board.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace altair {

class InterfacerThreeBoard : public Board {
public:
    InterfacerThreeBoard();
    ~InterfacerThreeBoard() override;

    std::string type() const override { return "interfacer3"; }

    // "CompuPro Interfacer 3 port 13 (if3)": the absolute user number, which CompuPro's
    // software calls the port (MP/M 8-16's TTYS: "Interfacer 3 port 7").
    std::string defaultUnitTitle(const std::string& unit) const override;

    bool    decodes(const BusCycle& c) const override;
    uint8_t read(const BusCycle& c) override;
    void    write(const BusCycle& c) override;
    bool    wantsSnoop() const override { return true; }  // the user select latch U34
    void    snoop(const BusCycle& c) override;

    // J15/J16 reach VI0-VI7 only.
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

    static constexpr int kUsers = 8;

    // Test access.
    uint8_t selectLatch() const { return latch_; }
    bool    selected() const { return usel(); }

private:
    struct User {
        explicit User(const std::string& n) : uart(n) {}
        Sig2651   uart;
        IrqJumper txIrq = IrqJumper::None;  // J15
        IrqJumper rxIrq = IrqJumper::None;  // J16
    };

    int  userIndex(const std::string& unit) const;  // -1 if no such unit
    bool usel() const { return ((latch_ >> 3) & 0x03) == boardSel_; }
    // S1-1 is the Board's own `enabled` property (enabled_): ON grounds ASEL.
    bool inBlock(uint16_t port) const { return enabled_ && (port & 0xF8) == base_; }
    void setLatch(uint8_t v);
    void logUart(User& u);

    void     refresh();
    uint64_t nextEdge() const;

    std::vector<std::unique_ptr<User>> users_;  // stable addresses: properties capture them

    uint8_t base_     = 0x10;  // S1-2..6 = A7-A3 (CompuPro standard 10H)
    int     boardSel_ = 0;     // S1-7/S1-8: users 0-7, 8-15, 16-23 or 24-31
    int     waits_    = 0;     // J17: 0, 1 or 2 wait states on a USART access
    uint8_t latch_    = 0;     // U34: D0-D4 of the last user select write
    uint64_t emptyPolls_ = 0;  // RxRDY port reads that found nothing (host bookkeeping)
    uint8_t txMask_   = 0;     // U38/U41: TxINTEN0-7
    uint8_t rxMask_   = 0;     // U52/U55: RxINTEN0-7

    Clock::Handle            wake_ = Clock::kNone;
    std::vector<std::string> log_;
};

} // namespace altair
