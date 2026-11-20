// Signetics SCN2681 DUART (src/chips/scn2681.h). Each check is a fact from
// reference/Signetics 2681 DUART.md, driven through the chip's registers on an 8 MHz clock
// (the SPUZ's Z80 clock, which is the clock the SPUZ attaches the chip to).

#include "chips/scn2681.h"
#include "core/clock.h"
#include "core/statefile.h"
#include "host/stream.h"
#include "test.h"

#include <memory>
#include <string>

using namespace altair;

namespace {

constexpr long long HZ = 8000000;

struct Rig {
    Clock    clk;
    Scn2681  u{"duart"};
    ScriptedStream* a = nullptr;
    ScriptedStream* b = nullptr;
    uint8_t  ip = 0x7F;

    Rig() {
        clk.setHz(HZ);
        auto sa = std::make_unique<ScriptedStream>();
        auto sb = std::make_unique<ScriptedStream>();
        a = sa.get();
        b = sb.get();
        u.connect(0, std::move(sa));
        u.connect(1, std::move(sb));
        u.inputPins = [this] { return ip; };
        u.powerOn(clk);
    }
    uint8_t rd(int r) {
        bool d;
        return u.read(r, clk, d);
    }
    bool driven(int r) {
        bool d;
        u.read(r, clk, d);
        return d;
    }
    void wr(int r, uint8_t v) { u.write(r, v, clk); }
    // Time passes and the card polls, as it does at the chip's nextEvent() and on pump().
    void run(uint64_t t) {
        clk.advance(t);
        u.poll(clk);
    }
    // Channel A at 9600 baud, 8 data bits, no parity, 1 stop bit: 10 bits a character.
    void line9600(uint8_t mr2 = 0x07) {
        wr(0x2, 0x10);   // reset MR pointer
        wr(0x0, 0x13);   // MR1A: 8 bits, no parity
        wr(0x0, mr2);    // MR2A
        wr(0x1, 0xBB);   // CSRA: 9600 both ways (ACR[7] = 0)
    }
};

constexpr uint64_t BIT = HZ * 10 / 96000;  // T-states per bit at 9600 baud (833)
constexpr uint64_t CHR = BIT * 10;

}  // namespace

void test_scn2681() {
    SECTION("2681 -- reset: OPR = 0 puts every OP pin high, IMR = 0, the MR pointer at MR1");
    {
        Rig r;
        CHECK(r.u.opPins(r.clk) == 0xFF, "OP0-OP7 high after reset (the complement of OPR = 0)");
        CHECK(!r.u.intrn(r.clk), "INTRN negated");
        CHECK((r.rd(0xD) & 0x80) == 0x80, "input port: D7 always reads 1");
        CHECK(r.rd(0xD) == 0xFF, "input port: IP6-IP0 as driven (all high)");
        r.wr(0x0, 0x11);  // MR1A
        r.wr(0x0, 0x22);  // MR2A (the pointer moved)
        r.wr(0x0, 0x33);  // MR2A again: the pointer stays at MR2
        CHECK(r.rd(0x0) == 0x33, "after MR1 the pointer stays at MR2");
        r.wr(0x2, 0x10);  // reset MR pointer
        CHECK(r.rd(0x0) == 0x11, "'reset MR pointer' brings MR1 back");
        CHECK(r.rd(0x0) == 0x33, "...and the read moved it to MR2");
        r.u.reset(r.clk);
        CHECK(r.rd(0x0) == 0x11, "RESET points the pointer at MR1 and keeps MR1");
    }

    SECTION("2681 -- the output port: set at E, reset at F, pins are the complement");
    {
        Rig r;
        r.wr(0xE, 0x4C);  // what SPZ.CMD writes: OPR bits 6, 3, 2
        CHECK(r.u.opr() == 0x4C, "set output port bits");
        CHECK(r.u.opPins(r.clk) == (uint8_t)~0x4C, "OP6, OP3, OP2 low; the others high");
        r.wr(0xF, 0x40);  // the SPUZ Z80 pulling its VI line: reset OPR[6]
        CHECK((r.u.opPins(r.clk) & 0x40) == 0x40, "OP6 high again");
        CHECK(r.u.opr() == 0x0C, "reset touched only bit 6");
    }

    SECTION("2681 -- reads whose data no sheet gives are not driven");
    {
        Rig r;
        CHECK(!r.driven(0x2) && !r.driven(0xA) && !r.driven(0xC), "2, A, C: test/reserved");
        CHECK(!r.driven(0xE) && !r.driven(0xF), "E, F: the counter commands");
        CHECK(r.driven(0x1) && r.driven(0x5) && r.driven(0xD), "status, ISR, input port are");
    }

    SECTION("2681 -- the SPUZ time slice: counter mode, X1/16, 3A00h, counter-ready interrupt");
    {
        Rig r;
        r.wr(0x4, 0xB0);  // ACR: BRG set 2, counter mode, X1/CLK / 16
        r.wr(0x6, 0x3A);
        r.wr(0x7, 0x00);
        r.wr(0x5, 0x08);  // IMR: counter ready only
        CHECK(!r.u.counterRunning(), "the counter is stopped after reset (the RESET pin text)");
        r.rd(0xE);        // start counter
        // 14,848 ticks of 230,400 Hz at 8 MHz: tick 14848 is at ceil(515,555.6) = 515,556.
        CHECK(r.u.nextEvent(r.clk) == 515556, "the next event is terminal count");
        r.run(515555);
        CHECK(!r.u.intrn(r.clk), "one T-state before terminal count: no interrupt");
        r.run(1);
        CHECK(r.u.intrn(r.clk), "at terminal count INTRN is asserted");
        CHECK((r.u.isr(r.clk) & 0x08) == 0x08, "ISR[3] counter ready");
        r.run(100000);
        CHECK(r.u.intrn(r.clk), "it stays asserted: the counter runs past terminal count");
        uint8_t hi = r.rd(0x6);
        CHECK(hi >= 0xF0, "past terminal count the count goes on down through FFFFh");
        r.rd(0xF);        // stop counter
        CHECK(!r.u.intrn(r.clk), "stop counter clears ISR[3] and INTRN");
        CHECK(!r.u.counterRunning(), "...and stops it in counter mode");
        uint8_t h1 = r.rd(0x6), l1 = r.rd(0x7);
        r.run(50000);
        CHECK(r.rd(0x6) == h1 && r.rd(0x7) == l1, "a stopped counter holds its count");
        r.rd(0xE);        // start again: a new cycle from CTUR/CTLR
        CHECK(r.rd(0x6) == 0x3A, "start counter reloads CTUR/CTLR");
    }

    SECTION("2681 -- timer mode: a square wave; stop counter clears ISR[3] and does not stop it");
    {
        Rig r;
        r.wr(0x4, 0x70);  // ACR: timer, X1/CLK / 16
        r.wr(0x6, 0x00);
        r.wr(0x7, 0x10);  // N = 16: a cycle is 32 ticks of 230,400 Hz
        r.wr(0xD, 0x04);  // OPCR[3:2] = 01: OP3 is the C/T output
        r.rd(0xE);
        uint64_t cycle = (32 * HZ + 230399) / 230400;  // 1,112 T-states
        CHECK((r.u.opPins(r.clk) & 0x08) == 0x08, "OP3 high in the first half period");
        r.run(cycle / 2 + 2);
        CHECK((r.u.opPins(r.clk) & 0x08) == 0, "OP3 low in the second half");
        CHECK((r.u.isr(r.clk) & 0x08) == 0, "no counter ready before a whole cycle");
        r.run(cycle / 2);
        CHECK((r.u.isr(r.clk) & 0x08) == 0x08, "counter ready once a cycle");
        r.rd(0xF);
        CHECK((r.u.isr(r.clk) & 0x08) == 0, "stop counter clears it");
        CHECK(r.u.counterRunning(), "...and the timer runs on");
        r.run(cycle + 2);
        CHECK((r.u.isr(r.clk) & 0x08) == 0x08, "the next cycle sets it again");
    }

    SECTION("2681 -- the transmitter: TxRDY back at the end of the start bit, TxEMT at the stop");
    {
        Rig r;
        r.line9600();
        r.wr(0x3, 'X');
        CHECK(r.a->out().empty(), "a disabled transmitter cannot be loaded");
        r.wr(0x2, 0x04);  // enable Tx
        CHECK((r.rd(0x1) & 0x0C) == 0x0C, "enabling sets TxRDY and TxEMT");
        r.wr(0x3, 'A');
        CHECK((r.rd(0x1) & 0x0C) == 0, "THR loaded: TxRDY and TxEMT reset");
        r.run(BIT - 1);
        CHECK(r.a->out().empty(), "still in the start bit: nothing committed to the line");
        r.run(1);
        CHECK(r.a->out() == "A", "past the start bit the byte is on the line");
        CHECK((r.rd(0x1) & 0x0C) == 0x04, "TxRDY back, TxEMT not yet");
        r.wr(0x3, 'B');   // waits behind A
        CHECK((r.rd(0x1) & 0x04) == 0, "THR full again");
        r.run(CHR - BIT);  // A's last stop bit: B starts
        r.run(BIT);
        CHECK(r.a->out() == "AB", "B follows A with no gap");
        r.run(CHR);
        CHECK((r.rd(0x1) & 0x0C) == 0x0C, "both empty: TxEMT");
    }

    SECTION("2681 -- disable in the start bit: the character is not sent (1998 disable note)");
    {
        Rig r;
        r.line9600();
        r.wr(0x2, 0x04);
        r.wr(0x3, 'Q');
        r.run(BIT / 2);
        r.wr(0x2, 0x08);  // disable Tx
        r.run(CHR * 2);
        CHECK(r.a->out().empty(), "a character still in its start bit is dropped");
        r.wr(0x2, 0x04);
        r.wr(0x3, 'R');
        r.run(BIT + 1);
        r.wr(0x3, 'S');   // waits behind R
        r.wr(0x2, 0x08);  // disable: R is on the line, S waits -- both go
        r.run(CHR * 3);
        CHECK(r.a->out() == "RS", "characters past the start bit, and one behind them, are sent");
        CHECK((r.rd(0x1) & 0x0C) == 0, "disabled: TxRDY and TxEMT are 0");
    }

    SECTION("2681 -- MR2[5]: OPR[0] (RTSAN) reset one bit time after the last stop bit");
    {
        Rig r;
        r.line9600(0x27);  // MR2A: Tx RTS control on
        r.wr(0x2, 0x04);
        r.wr(0xE, 0x01);   // assert RTSAN: OPR[0] = 1, OP0 low
        CHECK((r.u.opPins(r.clk) & 0x01) == 0, "OP0 low: RTSAN asserted");
        r.wr(0x3, 'M');
        r.run(BIT + 1);    // past the start bit (TxRDY back), as the 1998 note asks
        r.wr(0x2, 0x08);   // disable after the last character
        r.run(CHR - BIT);  // the last stop bit has gone
        CHECK((r.u.opPins(r.clk) & 0x01) == 0, "still asserted at the stop bit");
        r.run(BIT);
        CHECK((r.u.opPins(r.clk) & 0x01) == 0x01, "one bit time later OPR[0] is reset");
    }

    SECTION("2681 -- the receiver: a 3-character FIFO plus the shift register, no stream overrun");
    {
        Rig r;
        r.line9600();
        r.a->feed("1234");
        r.wr(0x2, 0x01);   // enable Rx
        r.run(CHR * 6);
        uint8_t s = r.rd(0x1);
        CHECK((s & 0x03) == 0x03, "RxRDY and FFULL");
        CHECK((s & 0x10) == 0, "no overrun: a byte stream waits instead");
        CHECK(r.rd(0x3) == '1', "first in, first out");
        CHECK((r.rd(0x1) & 0x02) == 0x02, "the character in the shift register moved in: FFULL");
        CHECK(r.rd(0x3) == '2' && r.rd(0x3) == '3' && r.rd(0x3) == '4', "the rest in order");
        CHECK((r.rd(0x1) & 0x01) == 0, "empty");
        CHECK(r.u.rxBytes(0) == 4, "four bytes crossed the line");
    }

    SECTION("2681 -- local loopback: overrun loses the character in the shift register");
    {
        Rig r;
        r.line9600(0x87);  // MR2A: local loopback
        r.wr(0x0, 0x87);
        r.wr(0x2, 0x05);   // enable Rx and Tx
        for (char ch : std::string("DEFGH")) {
            while (!(r.rd(0x1) & 0x04)) r.run(BIT / 4);
            r.wr(0x3, (uint8_t)ch);
        }
        r.run(CHR * 3);
        CHECK(r.a->out().empty(), "local loopback holds TxD at mark");
        CHECK((r.rd(0x1) & 0x10) == 0x10, "the fifth character overran");
        CHECK(r.rd(0x3) == 'D' && r.rd(0x3) == 'E' && r.rd(0x3) == 'F', "the FIFO kept D E F");
        CHECK(r.rd(0x3) == 'H', "G was lost; H took its place in the shift register");
        CHECK((r.rd(0x1) & 0x10) == 0x10, "overrun stays set");
        r.wr(0x2, 0x40);   // reset error status
        CHECK((r.rd(0x1) & 0x10) == 0, "until a reset error status command");
    }

    SECTION("2681 -- MR1[7]: the receiver negates RTSAN on a start bit with the FIFO full");
    {
        Rig r;
        r.line9600(0x87);
        r.wr(0x2, 0x10);
        r.wr(0x0, 0x93);   // MR1A: Rx RTS control, 8 bits, no parity
        r.wr(0x2, 0x05);
        r.wr(0xE, 0x01);   // OPR[0] = 1: RTSAN asserted
        for (char ch : std::string("abcd")) {
            while (!(r.rd(0x1) & 0x04)) r.run(BIT / 4);
            r.wr(0x3, (uint8_t)ch);
        }
        r.run(CHR * 2);
        CHECK((r.u.opPins(r.clk) & 0x01) == 0x01, "RTSAN negated (OP0 high)");
        CHECK(r.u.opr() & 0x01, "OPR[0] is not changed");
        r.rd(0x3);
        r.rd(0x3);
        CHECK((r.u.opPins(r.clk) & 0x01) == 0, "a free position asserts it again");
    }

    SECTION("2681 -- auto echo: received data is sent back; TxRDY and TxEMT inactive");
    {
        Rig r;
        r.line9600(0x47);  // MR2A: auto echo
        r.wr(0x2, 0x05);
        r.a->feed("hi");
        r.run(CHR * 3);
        CHECK(r.a->out() == "hi", "echoed on TxD");
        CHECK(r.rd(0x3) == 'h', "and still received by the CPU");
        CHECK((r.rd(0x1) & 0x0C) == 0, "TxRDY and TxEMT inactive");
    }

    SECTION("2681 -- multidrop: a disabled receiver wakes only on an address character");
    {
        Rig r;
        r.line9600(0x87);  // local loopback carries the A/D bit
        r.wr(0x2, 0x10);
        r.wr(0x0, 0x1B);   // MR1A: multidrop, A/D = 0 (data), 8 bits
        r.wr(0x2, 0x04);   // Tx only: the receiver is disabled
        r.wr(0x3, 'd');    // a data character
        r.run(CHR * 2);
        CHECK((r.rd(0x1) & 0x01) == 0, "a data character is discarded");
        r.wr(0x2, 0x10);
        r.wr(0x0, 0x1F);   // MR1A[2] = 1: the next character is an address
        r.wr(0x3, 'A');
        r.run(CHR * 2);
        CHECK((r.rd(0x1) & 0x21) == 0x21, "an address character is loaded; PE holds A/D = 1");
        CHECK(r.rd(0x3) == 'A', "the address");
    }

    SECTION("2681 -- input port change: IPCR deltas, ISR[7] with the ACR enable");
    {
        Rig r;
        r.wr(0x4, 0x01);   // ACR[0]: IP0 change interrupts
        r.wr(0x5, 0x80);   // IMR[7]
        r.ip = 0x7E;       // IP0 goes low
        CHECK(r.u.intrn(r.clk), "an enabled change asserts INTRN");
        uint8_t v = r.rd(0x4);
        CHECK(v == 0x1E, "IPCR: delta IP0, and IP3-IP0 now (1110)");
        CHECK(!r.u.intrn(r.clk), "a read of IPCR clears ISR[7]");
        r.ip = 0x7A;       // IP2 changes: not enabled
        CHECK(!r.u.intrn(r.clk), "a change that ACR does not enable does not interrupt");
        CHECK(r.rd(0x4) == 0x4A, "but IPCR records it");
    }

    SECTION("2681 -- OPCR: OP6 is the complement of TxRDYA, not masked by the IMR");
    {
        Rig r;
        r.line9600();
        r.wr(0xD, 0x40);
        CHECK((r.u.opPins(r.clk) & 0x40) == 0x40, "transmitter disabled: OP6 high");
        r.wr(0x2, 0x04);
        CHECK((r.u.opPins(r.clk) & 0x40) == 0, "TxRDYA: OP6 low, with IMR = 0");
    }

    SECTION("2681 -- the BRG test: each read of 2 toggles the rates (1998 Table 5)");
    {
        Rig r;
        r.wr(0x2, 0x10);
        r.wr(0x0, 0x13);
        r.wr(0x0, 0x07);
        r.wr(0x1, 0x00);   // CSRA code 0: 50 baud (ACR[7] = 0)
        CHECK(r.u.txBaud(0, r.clk) == 50, "normal: 50 baud");
        r.rd(0x2);
        CHECK(r.u.brgTest() && r.u.txBaud(0, r.clk) == 4800, "test: 4,800 baud");
        r.rd(0x2);
        CHECK(r.u.txBaud(0, r.clk) == 50, "a second read toggles it back");
        r.rd(0x2);
        r.u.reset(r.clk);
        CHECK(!r.u.brgTest(), "RESET clears the test mode");
    }

    SECTION("2681 -- snapshot: a character in flight and a running counter survive");
    {
        Rig r;
        r.line9600();
        r.wr(0x2, 0x04);
        r.wr(0x4, 0x30);
        r.wr(0x6, 0x01);
        r.wr(0x7, 0x00);
        r.rd(0xE);
        r.wr(0x3, 'Z');
        r.run(BIT / 2);
        StateWriter w;
        r.u.serialize(w);
        Rig q;
        q.clk.advance(r.clk.now());
        StateReader rd(w.data());
        q.u.deserialize(rd);
        q.run(BIT);
        CHECK(q.a->out() == "Z", "the restored chip finishes the character");
        CHECK(q.rd(0x6) == r.rd(0x6), "the counter is where it was");
    }
}
