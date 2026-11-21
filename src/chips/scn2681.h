#pragma once
//
// Signetics SCN2681 DUART -- a CHIP, NOT A CARD.
//
// The CompuPro SPUZ carries one (U30), and its Z80 is the only CPU that talks to it in normal
// operation. Modeled from reference/Signetics 2681 DUART.md (the Signetics data sheet the
// SPUZ manual reprints, and Philips' 1990 and 1998 SCN2681 specifications), NOT from the
// CompuPro code that drives it. CompuPro's SPZ.CMD uses only the output port, the input port
// and the counter; the serial channels are here anyway, because a chip built from one
// program's subset looks finished and is not (DESIGN.md 7.8, 3.0.4).
//
// It knows nothing about S-100 or about the SPUZ. It has a clock (whatever Clock its card
// attaches it to -- the SPUZ's private board clock), its own crystal on X1 (`x1Hz`), sixteen
// registers on A3-A0, seven input pins, eight output pins, an interrupt pin and two
// ByteStreams.
//
// TIME. Everything is computed from the attached Clock's now(): a character occupies a line
// for its frame time at the programmed baud rate, the counter/timer counts ticks of its
// source between two T-states. Nothing advances on its own. The card calls poll() before
// it looks at a pin, and asks nextEvent() for the next T-state at which a pin can move.
//
// THE CHOICES THE DATA SHEETS DO NOT MAKE are listed in the .cpp, each where it is made, and
// in docs/boards/compupro-spuz.md under Limitations. The large ones:
//   - a ByteStream carries bytes: no parity bit, no framing, no break, no A/D bit. So PE, FE
//     and received break stay 0 on a stream line, and a character from a stream has A/D = 0.
//     The channel's own loopback carries all of them;
//   - reads whose data no sheet gives (2, A, C, E, F) are reported as NOT DRIVEN, so the card
//     can let the bus float instead of inventing a byte (DESIGN.md 4.6.1);
//   - after RESET the counter/timer is stopped (the RESET pin text; the sheets disagree).

#include "core/clock.h"
#include "core/value.h"   // Property
#include "host/stream.h"  // ByteStream, LineParams

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace altair {

class Clock;
class StateWriter;
class StateReader;

// The monitor turns an endpoint string into a stream; the chip never learns the grammar
// (DESIGN.md 7.7). The same alias as chips/sig2651.h.
using EndpointResolver =
    std::function<std::unique_ptr<ByteStream>(const std::string&, std::string&)>;

class Scn2681 {
public:
    explicit Scn2681(std::string name);
    const std::string& name() const { return name_; }

    // ---- the bus side: A3-A0 = reg (0-15) ----
    // `driven` is false when the data sheets do not say what the chip puts on D0-D7: reads
    // of 2 and A (test registers), C (reserved), E and F (start/stop counter commands).
    uint8_t read(int reg, const Clock& clk, bool& driven);
    void    write(int reg, uint8_t v, const Clock& clk);

    // The RESET pin, high. Clears SRA, SRB, IMR, ISR, OPR, OPCR and the test modes; stops the
    // counter/timer; disables both channels (TxD to mark); points both MR pointers at MR1.
    // MR1, MR2, CSR, ACR, CTUR and CTLR keep their values.
    void reset(const Clock& clk);
    // Power: every register 0, then reset().
    void powerOn(const Clock& clk);

    // ---- the pins ----
    // IP6-IP0 as the card drives them, 1 = high. Unset: IP0/IP1 follow the two lines' CTS
    // (CTSAN/CTSBN, active low), IP2-IP6 read high (the internal pull-ups, 1998 pin table).
    std::function<uint8_t()> inputPins;
    // An external clock on IP2-IP6 (counter/timer IP2, TxCA IP3, RxCA IP4, TxCB IP5, RxCB IP6),
    // in Hz. Unset or 0: no clock on that pin, and whatever uses it stops.
    std::function<long long(int ip)> externalClockHz;
    long long x1Hz = 3686400;  // the crystal on X1/X2 (SPUZ: X2, 3.6864 MHz)

    uint8_t opPins(const Clock& clk);  // OP7-OP0 levels, 1 = high (OP[n] = NOT OPR[n] by default)
    bool    intrn(const Clock& clk);   // INTRN asserted (low): (ISR AND IMR) != 0
    uint8_t isr(const Clock& clk);     // the interrupt status, as a read of 5 would give it

    // Bring the chip up to clk.now(): characters that finished, the counter, the input
    // change detectors. The card calls it before it looks at a pin.
    void poll(const Clock& clk);
    // The next absolute T-state at which a pin or the ISR can change without a register
    // access, or 0 for none. The card sets a deadline on it.
    uint64_t nextEvent(const Clock& clk);

    // ---- the two lines (ch 0 = A, 1 = B) ----
    void        connect(int ch, std::unique_ptr<ByteStream> s);
    void        disconnect(int ch);
    ByteStream& stream(int ch) { return *chan_[ch].stream; }
    const ByteStream& stream(int ch) const { return *chan_[ch].stream; }
    void        pump();  // both lines
    uint64_t    rxBytes(int ch) const { return chan_[ch].rxCount; }
    std::vector<Property> channelProperties(int ch, const EndpointResolver& resolve);
    std::vector<std::string> drainLog();

    void serialize(StateWriter& w) const;
    void deserialize(StateReader& r);

    // ---- reflection, for the card's SHOW and for tests ----
    uint8_t opr() const { return opr_; }
    uint8_t imr() const { return imr_; }
    uint8_t acr() const { return acr_; }
    bool    counterRunning() const { return ctrRunning_; }
    bool    brgTest() const { return brgTest_; }
    long long txBaud(int ch, const Clock& clk) const;  // bits per second now, 0 = no clock

private:
    struct FifoEntry {
        uint8_t data  = 0;
        uint8_t flags = 0;  // SR bits 7:5 for this character (RB, FE, PE; PE = A/D in multidrop)
    };

    struct Channel {
        std::unique_ptr<ByteStream> stream;
        uint8_t mr1 = 0, mr2 = 0, csr = 0;
        bool    mrPtr2 = false;   // false: the next MR access reaches MR1

        // transmitter
        bool     txEn = false;
        bool     txDisablePending = false;  // disabled, still sending what it had
        bool     holdFull = false;
        uint8_t  holdByte = 0;
        bool     holdAD   = false;
        uint64_t holdAt   = 0;      // when the CPU wrote THR
        bool     shiftActive = false;
        bool     shiftCommitted = false;  // past the start bit: the byte is on the line
        uint8_t  shiftByte = 0;
        bool     shiftAD   = false;
        uint64_t shiftStart = 0, commitAt = 0, shiftEnd = 0;
        uint64_t lineFreeAt = 0;
        uint64_t rtsClearAt = 0;    // MR2[5]: OPR[0/1] is reset at this T-state (0 = none)
        bool     brk = false;       // start break / stop break

        // receiver
        bool     rxEn = false;
        bool     rxActive = false;  // a character is arriving from the stream
        uint8_t  rxByte = 0;
        uint64_t rxEnd = 0;
        uint64_t rxNextAt = 0;
        bool     held = false;      // a finished character waits in the shift register
        FifoEntry heldEntry;
        std::array<FifoEntry, 3> fifo{};
        int      count = 0;
        uint8_t  accum = 0;         // block error mode: OR of the flags of every top character
        bool     overrun = false;
        bool     deltaBreak = false;  // ISR[2]/[6]
        bool     rtsNeg = false;      // MR1[7]: RTSN negated by the receiver
        uint8_t  lastRead = 0;
        uint64_t rxCount = 0;

        LineControl driven;          // what was last sent to the stream's setControl
        bool        drivenValid = false;
    };

    // decode
    int      chMode(const Channel& c) const { return (c.mr2 >> 6) & 3; }  // 0 normal 1 echo 2 local 3 remote
    bool     multidrop(const Channel& c) const { return ((c.mr1 >> 3) & 3) == 3; }
    uint64_t bitT(int ch, bool rx, const Clock& clk) const;   // T-states per bit, 0 = no clock
    uint64_t charT(int ch, bool rx, const Clock& clk) const;  // a whole character
    long long counterSourceHz(const Clock& clk) const;

    // events
    void advance(const Clock& clk);
    void advanceChannel(int ch, const Clock& clk, uint64_t now);
    void deliver(int ch, uint8_t b, bool ad, uint8_t flags, const Clock& clk);
    void commitTx(int ch);
    void finishDisable(int ch, uint64_t at, const Clock& clk);
    void pollInputs();
    void driveLine(int ch, const Clock& clk);
    void programLine(int ch);  // the frame and rate, for a real serial port
    uint8_t pins() const;

    // registers
    uint8_t statusReg(int ch, const Clock& clk) const;
    bool    txRdy(int ch) const;
    bool    txEmt(int ch) const;
    uint8_t readRhr(int ch);
    void    command(int ch, uint8_t v, const Clock& clk);
    void    writeThr(int ch, uint8_t v, const Clock& clk);
    void    pushFifo(Channel& c, const FifoEntry& e);

    // counter/timer
    bool     timerMode() const { return (acr_ & 0x40) != 0; }
    uint64_t ctrTicks(uint64_t t, const Clock& clk) const;     // source ticks since ctrStart_
    uint64_t ctrTimeOfTick(uint64_t k, const Clock& clk) const;
    void     ctrSettle(const Clock& clk);                      // apply a pending timer reload
    bool     ctrReady(const Clock& clk) const { return ctrReadyAt(clk.now(), clk); }
    bool     ctrReadyAt(uint64_t t, const Clock& clk) const;
    uint16_t ctrValue(const Clock& clk) const;
    void     ctrStart(const Clock& clk);
    void     ctrStop(const Clock& clk);
    uint64_t ctrNextEvent(const Clock& clk) const;
    bool     ctrOutputLow(const Clock& clk) const;

    std::string name_;
    std::vector<std::string> log_;
    std::array<Channel, 2> chan_;

    uint8_t acr_ = 0, imr_ = 0, opr_ = 0, opcr_ = 0;
    uint8_t ctur_ = 0, ctlr_ = 0;
    uint8_t ipLast_ = 0x0F, ipDelta_ = 0;
    bool    isr7_ = false;
    bool    brgTest_ = false, test1x_ = false, test1xLogged_ = false;
    uint64_t prevPoll_ = 0;

    bool     ctrRunning_ = false;
    uint64_t ctrStart_ = 0;     // T-state of tick 0
    uint32_t ctrN_ = 0;         // the count in use (CTUR:CTLR at the last start)
    uint32_t ctrPhase_ = 0;     // timer: ticks already elapsed in the cycle at ctrStart_
    bool     ctrLatched_ = false;   // ISR[3] held set from before a re-anchor
    uint64_t ctrClearAt_ = 0;   // timer: the last stop counter command (ISR[3] cleared)
    uint16_t ctrFrozen_ = 0;    // counter mode: the count when stopped
    bool     ctrPending_ = false;   // timer: a new CTUR/CTLR takes effect at ctrPendingAt_
    uint64_t ctrPendingAt_ = 0;
    uint32_t ctrPendingN_ = 0;
    uint32_t ctrPendingPhase_ = 0;
};

}  // namespace altair
