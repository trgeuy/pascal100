#include "chips/scn2681.h"

#include "core/clock.h"
#include "core/statefile.h"

#include <algorithm>
#include <utility>

// Page references: "p. N" is the SPUZ manual's reprint of the Signetics data sheet; "1998" and
// "1990" are Philips' SCN2681 specifications. All three are distilled in
// reference/Signetics 2681 DUART.md, which this file follows section by section.

namespace altair {

namespace {

// Register numbers on A3-A0 (Table 1, p. 20).
enum : int {
    MRA = 0x0, SRA_CSRA = 0x1, TEST2_CRA = 0x2, RHRA_THRA = 0x3,
    IPCR_ACR = 0x4, ISR_IMR = 0x5, CTU_CTUR = 0x6, CTL_CTLR = 0x7,
    MRB = 0x8, SRB_CSRB = 0x9, TEST1X_CRB = 0xA, RHRB_THRB = 0xB,
    RESERVED_C = 0xC, IP_OPCR = 0xD, START_SET = 0xE, STOP_RESET = 0xF,
};

// Status register bits (Table 2, p. 21).
constexpr uint8_t kRxRDY = 0x01, kFFULL = 0x02, kTxRDY = 0x04, kTxEMT = 0x08, kOE = 0x10;
constexpr uint8_t kPE = 0x20;  // in multidrop mode: the received A/D bit (p. 24)

// The baud rate generator, in tenths of a baud (134.5 is exact). [ACR7][code 0-12]. Normal:
// CSRA table p. 23. BRG test (a read of 2 toggles it): Table 5, 1998 p. 24.
constexpr long long kBrg[2][13] = {
    {500, 1100, 1345, 2000, 3000, 6000, 12000, 10500, 24000, 48000, 72000, 96000, 384000},
    {750, 1100, 1345, 1500, 3000, 6000, 12000, 20000, 24000, 48000, 18000, 96000, 192000},
};
constexpr long long kBrgTest[2][13] = {
    {48000, 8800, 10760, 192000, 288000, 576000, 1152000, 10500, 576000, 48000, 576000, 96000,
     384000},
    {72000, 8800, 10760, 144000, 288000, 576000, 1152000, 20000, 576000, 48000, 144000, 96000,
     192000},
};

}  // namespace

Scn2681::Scn2681(std::string name) : name_(std::move(name)) {
    for (auto& c : chan_) c.stream = std::make_unique<NullStream>();
}

// ---------------------------------------------------------------------------
// CLOCKS. Each receiver and transmitter picks its clock in CSR (p. 23): one of 13 BRG rates,
// the counter/timer (1101), or an external pin at 16X (1110) or 1X (1111).
// ---------------------------------------------------------------------------
uint64_t Scn2681::bitT(int ch, bool rx, const Clock& clk) const {
    const Channel& c = chan_[ch];
    // Local loopback: the receiver runs on the transmit clock. Auto echo and remote loopback:
    // the transmitter runs on the receive clock (p. 22).
    bool useRx = rx;
    int  m     = chMode(c);
    if (m == 2 && rx) useRx = false;
    if ((m == 1 || m == 3) && !rx) useRx = true;
    int code = useRx ? (c.csr >> 4) : (c.csr & 0x0F);

    const long long hz = clk.hz();
    uint64_t t = 0;
    if (code <= 12) {
        long long b10 = (brgTest_ ? kBrgTest : kBrg)[(acr_ >> 7) & 1][code];
        t = (uint64_t)(hz * 10 / b10);
        if (test1x_) t /= 16;  // the 1X/16X test: the 16X clock shifts bits (1998 p. 24)
    } else if (code == 13) {
        // The timer's output is the 16X clock (p. 18): src / (2N) Hz. In counter mode, or
        // stopped, there is no square wave and so no clock.
        if (!timerMode() || !ctrRunning_ || ctrN_ == 0) return 0;
        long long src = counterSourceHz(clk);
        if (src <= 0) return 0;
        t = (uint64_t)(hz * (test1x_ ? 2 : 32) * (long long)ctrN_ / src);
    } else {
        int pin = (ch == 0) ? (useRx ? 4 : 3) : (useRx ? 6 : 5);
        long long e = externalClockHz ? externalClockHz(pin) : 0;
        if (e <= 0) return 0;  // no clock on the pin: the line does not shift
        t = (uint64_t)(hz * ((code == 14 && !test1x_) ? 16 : 1) / e);
    }
    return t ? t : 1;
}

// Start + data + parity (or the A/D bit) + stop. The stop length is in 1/16 bits (MR2[3:0],
// p. 21): 9/16..1 and 1-9/16..2, plus 1/2 for codes 0-7 at 5 bits; a 1X external transmit
// clock gives 1 or 2 (MR2[3], p. 23). The receiver checks only the first stop bit, so a
// received character is one stop bit long (p. 23).
uint64_t Scn2681::charT(int ch, bool rx, const Clock& clk) const {
    uint64_t bt = bitT(ch, rx, clk);
    if (!bt) return 0;
    const Channel& c = chan_[ch];
    int data = 5 + (c.mr1 & 3);
    int par  = (((c.mr1 >> 3) & 3) == 2) ? 0 : 1;  // 10 = no parity; multidrop sends A/D
    int stop16 = 16;
    if (!rx) {
        if ((c.csr & 0x0F) == 0x0F) {
            stop16 = (c.mr2 & 0x08) ? 32 : 16;
        } else {
            int code = c.mr2 & 0x0F;
            stop16   = code < 8 ? 9 + code : 25 + (code - 8);
            if (data == 5 && code < 8) stop16 += 8;
        }
    }
    return bt * (uint64_t)(1 + data + par) + bt * (uint64_t)stop16 / 16;
}

long long Scn2681::txBaud(int ch, const Clock& clk) const {
    const Channel& c = chan_[ch];
    int m    = chMode(c);
    int code = (m == 1 || m == 3) ? (c.csr >> 4) : (c.csr & 0x0F);
    if (code <= 12 && !test1x_)  // a BRG rate: the nominal figure, not one rounded to T-states
        return (brgTest_ ? kBrgTest : kBrg)[(acr_ >> 7) & 1][code] / 10;
    uint64_t bt = bitT(ch, false, clk);
    return bt ? clk.hz() / (long long)bt : 0;
}

// Table 4 (p. 26).
long long Scn2681::counterSourceHz(const Clock& clk) const {
    int sel = (acr_ >> 4) & 3;
    auto ext = [this] { return externalClockHz ? externalClockHz(2) : 0; };
    if (timerMode()) {
        switch (sel) {
        case 0:  return ext();
        case 1:  return ext() / 16;
        case 2:  return x1Hz;
        default: return x1Hz / 16;
        }
    }
    switch (sel) {
    case 0: return ext();
    case 1:
    case 2: {  // TxCA / TxCB: the 1X clock of that transmitter
        uint64_t bt = bitT(sel - 1, false, clk);
        return bt ? clk.hz() / (long long)bt : 0;
    }
    default: return x1Hz / 16;
    }
}

// ---------------------------------------------------------------------------
// THE COUNTER/TIMER (pp. 18, 26-27). It is not stepped: the count is the number of source
// ticks between ctrStart_ and now. Interpretations, from reference "The data sheets do not
// say": past terminal count the counter goes on down through FFFFh; in timer mode CTU/CTL
// read the count left in the current half period, and OP3 is high for the first half.
// ---------------------------------------------------------------------------
uint64_t Scn2681::ctrTicks(uint64_t t, const Clock& clk) const {
    if (t <= ctrStart_) return 0;
    long long src = counterSourceHz(clk);
    if (src <= 0) return 0;
    uint64_t dt = t - ctrStart_, hz = (uint64_t)clk.hz(), s = (uint64_t)src;
    return (dt / hz) * s + ((dt % hz) * s) / hz;
}

uint64_t Scn2681::ctrTimeOfTick(uint64_t k, const Clock& clk) const {
    long long src = counterSourceHz(clk);
    if (src <= 0) return 0;
    uint64_t hz = (uint64_t)clk.hz(), s = (uint64_t)src;
    return ctrStart_ + (k / s) * hz + ((k % s) * hz + s - 1) / s;
}

bool Scn2681::ctrReadyAt(uint64_t now, const Clock& clk) const {
    if (ctrLatched_) return true;
    if (!ctrRunning_) return false;
    if (!timerMode()) return ctrTicks(now, clk) >= ctrN_;  // terminal count reached
    if (ctrN_ == 0) return false;
    // Timer: set once each square-wave cycle (2N ticks); cleared by stop counter.
    uint64_t cyc   = 2ull * ctrN_;
    uint64_t kNow  = (ctrTicks(now, clk) + ctrPhase_) / cyc;
    uint64_t c     = std::max(ctrClearAt_, ctrStart_);
    uint64_t kClr  = (ctrTicks(c, clk) + ctrPhase_) / cyc;
    return kNow > kClr;
}

uint16_t Scn2681::ctrValue(const Clock& clk) const {
    if (!ctrRunning_) return ctrFrozen_;
    uint64_t e = ctrTicks(clk.now(), clk);
    if (!timerMode()) return (uint16_t)((ctrN_ - e) & 0xFFFF);
    if (ctrN_ == 0) return 0;
    return (uint16_t)(ctrN_ - ((e + ctrPhase_) % ctrN_));
}

bool Scn2681::ctrOutputLow(const Clock& clk) const {
    if (!timerMode()) return ctrReady(clk);  // low from terminal count until stop (p. 25)
    if (!ctrRunning_ || ctrN_ == 0) return false;
    uint64_t e = ctrTicks(clk.now(), clk) + ctrPhase_;
    return (e % (2ull * ctrN_)) >= ctrN_;
}

// A new CTUR/CTLR in timer mode takes effect at the next half-period boundary (p. 27).
void Scn2681::ctrSettle(const Clock& clk) {
    if (!ctrPending_ || clk.now() < ctrPendingAt_) return;
    // Everything up to the boundary happened under the old count: keep its ISR[3].
    bool r      = ctrReadyAt(ctrPendingAt_, clk);
    ctrStart_   = ctrPendingAt_;
    ctrN_       = ctrPendingN_;
    ctrPhase_   = ctrPendingPhase_ ? ctrN_ : 0;
    ctrClearAt_ = ctrPendingAt_;
    ctrLatched_ = r;
    ctrPending_ = false;
}

void Scn2681::ctrStart(const Clock& clk) {
    ctrSettle(clk);
    ctrLatched_ = ctrReady(clk);  // a start does not clear ISR[3]; only stop does (p. 26)
    ctrRunning_ = true;
    ctrStart_   = clk.now();
    ctrN_       = ((uint32_t)ctur_ << 8) | ctlr_;
    ctrPhase_   = 0;
    ctrClearAt_ = clk.now();
    ctrPending_ = false;
}

void Scn2681::ctrStop(const Clock& clk) {
    ctrSettle(clk);
    if (!timerMode()) {
        if (ctrRunning_) ctrFrozen_ = ctrValue(clk);
        ctrRunning_ = false;     // counter mode: it stops, OP3 returns high
    }
    ctrLatched_ = false;         // ISR[3] is cleared; in timer mode the timer runs on
    ctrClearAt_ = clk.now();
}

uint64_t Scn2681::ctrNextEvent(const Clock& clk) const {
    uint64_t now = clk.now(), best = 0;
    auto take = [&](uint64_t t) {
        if (t > now && (!best || t < best)) best = t;
    };
    if (ctrPending_) take(ctrPendingAt_);
    if (!ctrRunning_ || counterSourceHz(clk) <= 0) return best;
    bool opOut = ((opcr_ >> 2) & 3) == 1;
    if (!timerMode()) {
        if (!ctrReady(clk)) take(ctrTimeOfTick(ctrN_, clk));
        return best;
    }
    if (ctrN_ == 0) return best;
    if (!opOut && ctrReady(clk)) return best;  // nothing visible moves until a stop command
    uint64_t e = ctrTicks(now, clk) + ctrPhase_;
    uint64_t b = (e / ctrN_ + 1) * ctrN_;      // the next half-period boundary
    take(ctrTimeOfTick(b - ctrPhase_, clk));
    return best;
}

// ---------------------------------------------------------------------------
// THE PINS.
// ---------------------------------------------------------------------------
uint8_t Scn2681::pins() const {
    if (inputPins) return inputPins() & 0x7F;
    uint8_t v = 0x7C;                                       // IP2-IP6: pulled up
    if (!chan_[0].stream->status().cts) v |= 0x01;          // CTSAN is active low
    if (!chan_[1].stream->status().cts) v |= 0x02;          // CTSBN
    return v;
}

// IP3-IP0 change-of-state (p. 18). The data sheets debounce over two 38.4 kHz samples
// (25-50 us); a change seen at a poll is taken at once.
void Scn2681::pollInputs() {
    uint8_t cur = pins() & 0x0F;
    uint8_t chg = cur ^ ipLast_;
    if (chg) {
        ipDelta_ |= (uint8_t)(chg << 4);
        if (chg & acr_ & 0x0F) isr7_ = true;
        ipLast_ = cur;
    }
}

bool Scn2681::txRdy(int ch) const {
    const Channel& c = chan_[ch];
    int m = chMode(c);
    if (m == 1 || m == 3) return false;  // auto echo: TxRDY/TxEMT inactive (p. 22)
    return c.txEn && !c.holdFull;
}

// TxEMT: the transmitter has underrun -- both registers empty. It is set when the transmitter
// is enabled and after the last stop bit with nothing in THR; it is reset when THR is loaded
// or the transmitter is disabled (1998 p. 14).
bool Scn2681::txEmt(int ch) const {
    const Channel& c = chan_[ch];
    int m = chMode(c);
    if (m == 1 || m == 3) return false;
    return c.txEn && !c.holdFull && !c.shiftActive;
}

uint8_t Scn2681::isr(const Clock& clk) {
    poll(clk);
    const Channel& a = chan_[0];
    const Channel& b = chan_[1];
    uint8_t v = 0;
    if (txRdy(0)) v |= 0x01;
    if ((a.mr1 & 0x40) ? a.count == 3 : a.count > 0) v |= 0x02;
    if (a.deltaBreak) v |= 0x04;
    if (ctrReady(clk)) v |= 0x08;
    if (txRdy(1)) v |= 0x10;
    if ((b.mr1 & 0x40) ? b.count == 3 : b.count > 0) v |= 0x20;
    if (b.deltaBreak) v |= 0x40;
    if (isr7_) v |= 0x80;
    return v;
}

bool Scn2681::intrn(const Clock& clk) { return (isr(clk) & imr_) != 0; }

// OP[n] is the complement of OPR[n], unless OPCR or the MRs give the pin a function (p. 25,
// 1998 "Output Port Notes"). A clock output (OP2, OP3 codes 10/11) cannot be shown as a level
// between instructions: such a pin reads high here.
uint8_t Scn2681::opPins(const Clock& clk) {
    uint8_t is = isr(clk);  // polls
    uint8_t p  = 0;
    for (int ch = 0; ch < 2; ++ch) {
        const Channel& c = chan_[ch];
        bool rxRts = !((c.mr1 & 0x80) && c.rtsNeg);
        bool low   = ((opr_ >> ch) & 1) && rxRts;  // a NAND of OPR and the receiver's RTS
        if (!low) p |= (uint8_t)(1 << ch);
    }
    if ((opcr_ & 3) == 0 ? !(opr_ & 0x04) : true) p |= 0x04;
    switch ((opcr_ >> 2) & 3) {
    case 0:  if (!(opr_ & 0x08)) p |= 0x08; break;
    case 1:  if (!ctrOutputLow(clk)) p |= 0x08; break;
    default: p |= 0x08; break;
    }
    if ((opcr_ & 0x10) ? !(is & 0x02) : !(opr_ & 0x10)) p |= 0x10;
    if ((opcr_ & 0x20) ? !(is & 0x20) : !(opr_ & 0x20)) p |= 0x20;
    if ((opcr_ & 0x40) ? !txRdy(0) : !(opr_ & 0x40)) p |= 0x40;
    if ((opcr_ & 0x80) ? !txRdy(1) : !(opr_ & 0x80)) p |= 0x80;
    return p;
}

// What the line sees. RTS is OP0/OP1 (RTSAN/RTSBN, active low). The 2681 has no DTR pin; the
// line's DTR is held asserted so that a socket endpoint stays up (interpretation).
void Scn2681::driveLine(int ch, const Clock& clk) {
    Channel& c = chan_[ch];
    bool rxRts = !((c.mr1 & 0x80) && c.rtsNeg);
    LineControl lc;
    lc.rts = ((opr_ >> ch) & 1) && rxRts;
    lc.dtr = true;
    lc.brk = c.brk && chMode(c) != 2;  // local loopback holds TxD at mark
    (void)clk;
    if (c.drivenValid && lc.rts == c.driven.rts && lc.dtr == c.driven.dtr &&
        lc.brk == c.driven.brk)
        return;
    c.driven      = lc;
    c.drivenValid = true;
    c.stream->setControl(lc);
}

// ---------------------------------------------------------------------------
// THE CHARACTERS. advanceChannel() takes a channel's events in time order up to now: a
// transmit character starting, passing its start bit (the byte is committed to the line and
// THR is empty again: "TxRDY sets at the end of the start bit time", 1998 p. 21), ending; a
// received character starting and ending; the MR2[5] RTS reset.
// ---------------------------------------------------------------------------
void Scn2681::commitTx(int ch) {
    Channel& c = chan_[ch];
    c.holdFull       = false;
    c.shiftCommitted = true;
    if (chMode(c) != 2) {          // local loopback: TxD stays at mark (p. 22)
        uint8_t b = c.shiftByte;
        c.stream->write(&b, 1);
        c.stream->flush();
    }
}

void Scn2681::finishDisable(int ch, uint64_t at, const Clock& clk) {
    Channel& c = chan_[ch];
    c.txDisablePending = false;
    if (c.mr2 & 0x20) {  // MR2[5]: OPR[n] reset one bit time after the last stop bit (p. 23)
        uint64_t bt  = bitT(ch, false, clk);
        c.rtsClearAt = std::max(at, c.lineFreeAt) + bt;
        if (!c.rtsClearAt) c.rtsClearAt = 1;
    }
}

void Scn2681::pushFifo(Channel& c, const FifoEntry& e) {
    if (c.count == 0) c.accum |= e.flags;  // it comes to the top of the FIFO
    c.fifo[c.count++] = e;
}

// A character has arrived at the receiver (pp. 19-20, 22).
void Scn2681::deliver(int ch, uint8_t b, bool ad, uint8_t flags, const Clock& clk) {
    Channel& c = chan_[ch];
    int m = chMode(c);
    (void)clk;
    if (m == 1 || m == 3) {        // auto echo, remote loopback: retransmitted on TxD
        c.stream->write(&b, 1);
        c.stream->flush();
    }
    if (m == 3) return;            // remote loopback: not sent to the CPU
    if (multidrop(c)) {
        if (ad) flags |= kPE;      // the A/D bit is stored where parity error would be
        if (!c.rxEn && !ad) return;  // disabled: only an address character wakes it
    } else if (!c.rxEn) {
        return;
    }
    FifoEntry e{b, flags};
    if (c.count < 3) {
        pushFifo(c, e);
    } else if (!c.held) {
        c.held      = true;        // waits in the shift register; FFULL stays set
        c.heldEntry = e;
    } else {
        c.overrun   = true;        // the waiting character is lost (p. 19)
        c.heldEntry = e;
    }
}

void Scn2681::advanceChannel(int ch, const Clock& clk, uint64_t now) {
    Channel& c = chan_[ch];
    for (int guard = 0; guard < 4096; ++guard) {
        int      m     = chMode(c);
        int      which = 0;
        uint64_t t     = 0;
        auto cand = [&](int w, uint64_t at) {
            if (at <= now && (which == 0 || at < t)) {
                which = w;
                t     = at;
            }
        };
        if (c.shiftActive && !c.shiftCommitted) cand(1, c.commitAt);
        if (c.shiftActive && c.shiftCommitted) cand(2, c.shiftEnd);
        bool canSend = (c.txEn || c.txDisablePending) && (m == 0 || m == 2);
        bool ctsOk   = !(c.mr2 & 0x10) || !((pins() >> ch) & 1);  // CTSN low = clear (p. 23)
        if (!c.shiftActive && c.holdFull && canSend && ctsOk && bitT(ch, false, clk))
            cand(3, std::max(c.lineFreeAt, c.holdAt));
        if (c.rxActive) cand(4, c.rxEnd);
        if (m != 2 && (c.rxEn || multidrop(c)) && !c.rxActive && !c.held &&
            charT(ch, true, clk) && c.stream->readable())
            cand(5, std::max(c.rxNextAt, prevPoll_));
        if (c.rtsClearAt) cand(6, c.rtsClearAt);
        if (!which) return;

        switch (which) {
        case 1:  // past the start bit: on the line, THR empty again
            commitTx(ch);
            break;
        case 2:  // the last stop bit has gone
            c.shiftActive = false;
            c.lineFreeAt  = c.shiftEnd;
            if (m == 2) deliver(ch, c.shiftByte, c.shiftAD, 0, clk);  // local loopback
            if (c.txDisablePending && !c.holdFull) finishDisable(ch, c.shiftEnd, clk);
            break;
        case 3:  // THR into the shift register: the start bit begins
            c.shiftActive    = true;
            c.shiftCommitted = false;
            c.shiftByte      = c.holdByte;
            c.shiftAD        = c.holdAD;
            c.shiftStart     = t;
            c.commitAt       = t + bitT(ch, false, clk);
            c.shiftEnd       = t + charT(ch, false, clk);
            if (m == 2 && c.count == 3 && (c.mr1 & 0x80)) c.rtsNeg = true;
            break;
        case 4:  // a character from the line is in
            c.rxActive = false;
            c.rxNextAt = c.rxEnd;  // it looks for the next start bit at once (p. 19)
            deliver(ch, c.rxByte, false, 0, clk);  // a byte stream carries no A/D bit
            break;
        case 5: {  // a start bit on the line
            uint8_t b = 0;
            if (c.stream->read(&b, 1) != 1) return;
            c.rxActive = true;
            c.rxByte   = b;
            c.rxEnd    = t + charT(ch, true, clk);
            ++c.rxCount;
            if (c.count == 3 && (c.mr1 & 0x80)) c.rtsNeg = true;  // MR1[7] (p. 20)
            break;
        }
        case 6:
            opr_ &= (uint8_t)~(1 << ch);
            c.rtsClearAt = 0;
            break;
        }
    }
}

void Scn2681::advance(const Clock& clk) {
    for (int ch = 0; ch < 2; ++ch) advanceChannel(ch, clk, clk.now());
}

void Scn2681::poll(const Clock& clk) {
    ctrSettle(clk);
    advance(clk);
    pollInputs();
    driveLine(0, clk);
    driveLine(1, clk);
    prevPoll_ = clk.now();
}

uint64_t Scn2681::nextEvent(const Clock& clk) {
    uint64_t now = clk.now(), best = 0;
    auto take = [&](uint64_t t) {
        if (t > now && (!best || t < best)) best = t;
    };
    for (int ch = 0; ch < 2; ++ch) {
        const Channel& c = chan_[ch];
        int m = chMode(c);
        if (c.shiftActive) take(c.shiftCommitted ? c.shiftEnd : c.commitAt);
        if (!c.shiftActive && c.holdFull && (c.txEn || c.txDisablePending) &&
            (m == 0 || m == 2))
            take(std::max(c.lineFreeAt, c.holdAt));
        if (c.rxActive) take(c.rxEnd);
        if (c.rtsClearAt) take(c.rtsClearAt);
    }
    take(ctrNextEvent(clk));
    return best;
}

// ---------------------------------------------------------------------------
// REGISTERS.
// ---------------------------------------------------------------------------
uint8_t Scn2681::statusReg(int ch, const Clock& clk) const {
    const Channel& c = chan_[ch];
    (void)clk;
    uint8_t v = 0;
    if (chMode(c) != 3) {  // remote loopback: the error status conditions are inactive
        if (c.mr1 & 0x20) v |= c.accum & 0xE0;                       // block mode
        else if (c.count > 0) v |= c.fifo[0].flags & 0xE0;           // char mode: the top
    }
    if (c.overrun) v |= kOE;
    if (txEmt(ch)) v |= kTxEMT;
    if (txRdy(ch)) v |= kTxRDY;
    if (c.count == 3) v |= kFFULL;
    if (c.count > 0) v |= kRxRDY;
    return v;
}

// A read of RHR pops the FIFO; a character waiting in the shift register moves in. Reading an
// empty FIFO: the sheets do not say; this returns the last character read (interpretation).
uint8_t Scn2681::readRhr(int ch) {
    Channel& c = chan_[ch];
    if (c.count == 0) return c.lastRead;
    FifoEntry top = c.fifo[0];
    c.fifo[0] = c.fifo[1];
    c.fifo[1] = c.fifo[2];
    --c.count;
    if (c.count > 0) c.accum |= c.fifo[0].flags;  // a new character comes to the top
    if (c.held) {
        c.held = false;
        pushFifo(c, c.heldEntry);
    }
    if (c.count < 3) c.rtsNeg = false;  // a FIFO position is free: RTSN asserted again
    c.lastRead = top.data;
    return top.data;
}

void Scn2681::writeThr(int ch, uint8_t v, const Clock& clk) {
    Channel& c = chan_[ch];
    int m = chMode(c);
    if (!c.txEn) return;              // a disabled transmitter cannot be loaded (p. 19)
    if (m == 1 || m == 3) return;     // the CPU-to-transmitter link is disabled (p. 22)
    // A write while THR is full replaces it; during the start bit the replaced byte is lost
    // (interpretation: the sheets say only to wait for TxRDY).
    c.holdFull = true;
    c.holdByte = v;
    c.holdAD   = (c.mr1 & 0x04) != 0;  // MR1[2]: the A/D bit, set before loading (p. 20)
    c.holdAt   = clk.now();
}

void Scn2681::command(int ch, uint8_t v, const Clock& clk) {
    Channel& c   = chan_[ch];
    uint64_t now = clk.now();
    switch ((v >> 4) & 7) {
    case 1: c.mrPtr2 = false; break;
    case 2:  // reset receiver: as a hardware reset of the receiver; FIFO flushed
        c.rxEn = c.rxActive = c.held = false;
        c.count = 0;
        c.overrun = c.deltaBreak = c.rtsNeg = false;
        c.accum = 0;
        c.rxNextAt = now;
        break;
    case 3:  // reset transmitter: stops at once, must be enabled again
        c.txEn = c.txDisablePending = c.holdFull = c.shiftActive = false;
        c.brk = false;
        c.rtsClearAt = 0;
        c.lineFreeAt = now;
        break;
    case 4:  // reset error status: RB, FE, PE, OE (p. 24)
        c.overrun = false;
        c.accum   = 0;
        for (auto& e : c.fifo) e.flags = 0;
        c.heldEntry.flags = 0;
        break;
    case 5: c.deltaBreak = false; break;
    case 6:  // start break. The line is told at once; the sheets delay it until the
        if (c.txEn) c.brk = true;  // characters in hand have gone (interpretation).
        break;
    case 7: c.brk = false; break;
    default: break;
    }
    // Enable and disable in one write conflict (p. 24). Order taken here: enable, then
    // disable -- the receiver ends disabled; the transmitter is the other way round
    // (disable, then enable), so it ends enabled (interpretation).
    if (v & 0x01) {
        if (!c.rxEn && !multidrop(c) && !c.rxActive) c.rxNextAt = now;
        c.rxEn = true;
    }
    if (v & 0x02) {
        c.rxEn     = false;
        c.rxActive = false;  // a character being received is lost (p. 24)
    }
    if (v & 0x08) {
        if (c.txEn) {
            c.txEn = false;
            // A character still in its start bit is not sent (1998 "Transmitter Disable
            // Note"); one already on the line, or one waiting behind it, is.
            if (c.shiftActive && !c.shiftCommitted) {
                c.shiftActive = false;
                c.holdFull    = false;
            }
            if (c.shiftActive || c.holdFull) c.txDisablePending = true;
            else finishDisable(ch, now, clk);
        }
    }
    if (v & 0x04) {
        c.txEn = true;
        c.txDisablePending = false;
        c.rtsClearAt = 0;
    }
}

uint8_t Scn2681::read(int reg, const Clock& clk, bool& driven) {
    poll(clk);
    driven = true;
    switch (reg & 0x0F) {
    case MRA:
    case MRB: {
        Channel& c = chan_[reg >> 3];
        uint8_t v  = c.mrPtr2 ? c.mr2 : c.mr1;
        c.mrPtr2   = true;  // a read or a write of MR1 moves the pointer to MR2 (p. 20)
        return v;
    }
    case SRA_CSRA: return statusReg(0, clk);
    case SRB_CSRB: return statusReg(1, clk);
    case TEST2_CRA:
        brgTest_ = !brgTest_;  // each read toggles the BRG test mode (1998 p. 24)
        for (int ch = 0; ch < 2; ++ch) programLine(ch);
        driven = false;
        return 0xFF;
    case TEST1X_CRB:
        test1x_ = true;
        if (!test1xLogged_) {
            log_.push_back(name_ + ": a read of register A entered the 1X/16X test mode; its "
                                   "effect on the output port is not modelled");
            test1xLogged_ = true;
        }
        driven = false;
        return 0xFF;
    case RHRA_THRA: { uint8_t v = readRhr(0); poll(clk); return v; }
    case RHRB_THRB: { uint8_t v = readRhr(1); poll(clk); return v; }
    case IPCR_ACR: {
        uint8_t v = (uint8_t)(ipDelta_ | (pins() & 0x0F));
        ipDelta_  = 0;
        isr7_     = false;  // a read of IPCR clears ISR[7] (p. 26)
        return v;
    }
    case ISR_IMR: return isr(clk);
    case CTU_CTUR: return (uint8_t)(ctrValue(clk) >> 8);
    case CTL_CTLR: return (uint8_t)(ctrValue(clk) & 0xFF);
    case RESERVED_C: driven = false; return 0xFF;
    case IP_OPCR: return (uint8_t)(0x80 | pins());  // D7 always reads 1 (p. 18)
    case START_SET:
        ctrStart(clk);
        driven = false;
        return 0xFF;
    case STOP_RESET:
    default:
        ctrStop(clk);
        driven = false;
        return 0xFF;
    }
}

void Scn2681::write(int reg, uint8_t v, const Clock& clk) {
    poll(clk);
    switch (reg & 0x0F) {
    case MRA:
    case MRB: {
        int ch     = reg >> 3;
        Channel& c = chan_[ch];
        if (c.mrPtr2) c.mr2 = v;
        else c.mr1 = v;
        c.mrPtr2 = true;
        programLine(ch);
        break;
    }
    case SRA_CSRA: chan_[0].csr = v; programLine(0); break;
    case SRB_CSRB: chan_[1].csr = v; programLine(1); break;
    case TEST2_CRA: command(0, v, clk); break;
    case TEST1X_CRB: command(1, v, clk); break;
    case RHRA_THRA: writeThr(0, v, clk); break;
    case RHRB_THRB: writeThr(1, v, clk); break;
    case IPCR_ACR: {
        // Changes "should only be made while the C/T is stopped" (p. 20). If it runs, the
        // count goes on from now under the new mode and source (interpretation).
        ctrSettle(clk);
        bool     r = ctrReady(clk);
        uint16_t cur = ctrValue(clk);
        acr_ = v;
        if (ctrRunning_) {
            ctrStart_   = clk.now();
            ctrN_       = timerMode() ? (((uint32_t)ctur_ << 8) | ctlr_) : cur;
            ctrPhase_   = 0;
            ctrClearAt_ = clk.now();
            ctrLatched_ = r;
            ctrPending_ = false;
        }
        for (int ch = 0; ch < 2; ++ch) programLine(ch);
        break;
    }
    case ISR_IMR: imr_ = v; break;
    case CTU_CTUR:
    case CTL_CTLR: {
        if ((reg & 0x0F) == CTU_CTUR) ctur_ = v;
        else ctlr_ = v;
        // Counter mode: effective at the next start. Timer mode: at the next half period.
        if (timerMode() && ctrRunning_ && ctrN_) {
            ctrSettle(clk);
            if (!ctrPending_) {
                uint64_t e      = ctrTicks(clk.now(), clk) + ctrPhase_;
                uint64_t b      = (e / ctrN_ + 1) * ctrN_;
                ctrPendingAt_   = ctrTimeOfTick(b - ctrPhase_, clk);
                ctrPendingPhase_ = (uint32_t)((b / ctrN_) & 1);
                ctrPending_     = true;
            }
            ctrPendingN_ = ((uint32_t)ctur_ << 8) | ctlr_;
        }
        break;
    }
    case RESERVED_C: break;
    case IP_OPCR: opcr_ = v; break;
    case START_SET: opr_ |= v; break;
    case STOP_RESET:
    default: opr_ &= (uint8_t)~v; break;
    }
    driveLine(0, clk);
    driveLine(1, clk);
}

// ---------------------------------------------------------------------------
// RESET AND POWER.
// ---------------------------------------------------------------------------
void Scn2681::reset(const Clock& clk) {
    uint64_t now = clk.now();
    for (auto& c : chan_) {
        c.mrPtr2 = false;
        c.txEn = c.txDisablePending = c.holdFull = c.shiftActive = c.shiftCommitted = false;
        c.lineFreeAt = now;
        c.rtsClearAt = 0;
        c.brk = false;
        c.rxEn = c.rxActive = c.held = false;
        c.count = 0;
        c.accum = 0;
        c.overrun = c.deltaBreak = c.rtsNeg = false;
        c.rxNextAt = now;
        c.drivenValid = false;
    }
    imr_ = opr_ = opcr_ = 0;
    isr7_ = false;
    brgTest_ = test1x_ = false;
    // Stopped: the RESET pin text of every SCN2681 sheet (the counter section disagrees;
    // reference "Reset").
    if (ctrRunning_ && !timerMode()) ctrFrozen_ = ctrValue(clk);
    ctrRunning_ = ctrLatched_ = ctrPending_ = false;
    ctrClearAt_ = now;
    prevPoll_   = now;
    for (int ch = 0; ch < 2; ++ch) {
        programLine(ch);
        driveLine(ch, clk);
    }
}

void Scn2681::powerOn(const Clock& clk) {
    for (auto& c : chan_) {
        c.mr1 = c.mr2 = c.csr = 0;
        c.lastRead = 0;
    }
    acr_ = ctur_ = ctlr_ = 0;
    ipDelta_ = 0;
    ctrStart_ = 0;
    ctrN_ = ctrPhase_ = 0;
    ctrFrozen_ = 0;
    ctrRunning_ = false;
    test1xLogged_ = false;
    reset(clk);
    ipLast_ = pins() & 0x0F;
}

// ---------------------------------------------------------------------------
// THE LINES.
// ---------------------------------------------------------------------------
void Scn2681::programLine(int ch) {
    Channel& c = chan_[ch];
    int code   = c.csr & 0x0F;
    if (code > 12) return;  // the timer or a pin: no fixed rate to give a real port
    LineParams p;
    p.baud     = (brgTest_ ? kBrgTest : kBrg)[(acr_ >> 7) & 1][code] / 10;
    p.dataBits = 5 + (c.mr1 & 3);
    int pm     = (c.mr1 >> 3) & 3;
    p.parity   = (pm == 0) ? ((c.mr1 & 0x04) ? LineParity::Odd : LineParity::Even)
                           : LineParity::None;
    p.stopBits = ((c.mr2 & 0x0F) >= 8) ? 2 : 1;
    std::string err;
    if (!c.stream->setParams(p, err)) log_.push_back(name_ + ": " + err);
}

void Scn2681::connect(int ch, std::unique_ptr<ByteStream> s) {
    chan_[ch].stream      = std::move(s);
    chan_[ch].drivenValid = false;
    programLine(ch);
}

void Scn2681::disconnect(int ch) { connect(ch, std::make_unique<NullStream>()); }

void Scn2681::pump() {
    for (auto& c : chan_) c.stream->pump();
}

std::vector<std::string> Scn2681::drainLog() {
    auto out = std::move(log_);
    log_.clear();
    return out;
}

std::vector<Property> Scn2681::channelProperties(int ch, const EndpointResolver& resolve) {
    std::vector<Property> p;
    Property x;
    x.name = "connect";
    x.help = "The endpoint on the other end of this channel's line (CONNECT sets this)";
    x.kind = Kind::Str;
    x.get  = [this, ch] { return Value::ofStr(chan_[ch].stream->describe()); };
    x.set  = [this, ch, resolve](const Value& v, std::string& err) {
        if (!resolve) {
            err = "no endpoint resolver installed";
            return false;
        }
        auto s = resolve(v.s(), err);
        if (!s) return false;
        connect(ch, std::move(s));
        return true;
    };
    p.push_back(std::move(x));
    return p;
}

// ---------------------------------------------------------------------------
// SNAPSHOT. Every deadline is an absolute T-state on the attached clock, which travels.
// ---------------------------------------------------------------------------
void Scn2681::serialize(StateWriter& w) const {
    for (const auto& c : chan_) {
        w.u8(c.mr1); w.u8(c.mr2); w.u8(c.csr); w.boolean(c.mrPtr2);
        w.boolean(c.txEn); w.boolean(c.txDisablePending);
        w.boolean(c.holdFull); w.u8(c.holdByte); w.boolean(c.holdAD); w.u64(c.holdAt);
        w.boolean(c.shiftActive); w.boolean(c.shiftCommitted); w.u8(c.shiftByte);
        w.boolean(c.shiftAD); w.u64(c.shiftStart); w.u64(c.commitAt); w.u64(c.shiftEnd);
        w.u64(c.lineFreeAt); w.u64(c.rtsClearAt); w.boolean(c.brk);
        w.boolean(c.rxEn); w.boolean(c.rxActive); w.u8(c.rxByte); w.u64(c.rxEnd);
        w.u64(c.rxNextAt); w.boolean(c.held); w.u8(c.heldEntry.data); w.u8(c.heldEntry.flags);
        for (const auto& e : c.fifo) { w.u8(e.data); w.u8(e.flags); }
        w.u32((uint32_t)c.count); w.u8(c.accum); w.boolean(c.overrun);
        w.boolean(c.deltaBreak); w.boolean(c.rtsNeg); w.u8(c.lastRead); w.u64(c.rxCount);
    }
    w.u8(acr_); w.u8(imr_); w.u8(opr_); w.u8(opcr_); w.u8(ctur_); w.u8(ctlr_);
    w.u8(ipLast_); w.u8(ipDelta_); w.boolean(isr7_);
    w.boolean(brgTest_); w.boolean(test1x_); w.u64(prevPoll_);
    w.boolean(ctrRunning_); w.u64(ctrStart_); w.u32(ctrN_); w.u32(ctrPhase_);
    w.boolean(ctrLatched_); w.u64(ctrClearAt_); w.u16(ctrFrozen_);
    w.boolean(ctrPending_); w.u64(ctrPendingAt_); w.u32(ctrPendingN_); w.u32(ctrPendingPhase_);
}

void Scn2681::deserialize(StateReader& r) {
    for (auto& c : chan_) {
        c.mr1 = r.u8(); c.mr2 = r.u8(); c.csr = r.u8(); c.mrPtr2 = r.boolean();
        c.txEn = r.boolean(); c.txDisablePending = r.boolean();
        c.holdFull = r.boolean(); c.holdByte = r.u8(); c.holdAD = r.boolean(); c.holdAt = r.u64();
        c.shiftActive = r.boolean(); c.shiftCommitted = r.boolean(); c.shiftByte = r.u8();
        c.shiftAD = r.boolean(); c.shiftStart = r.u64(); c.commitAt = r.u64();
        c.shiftEnd = r.u64(); c.lineFreeAt = r.u64(); c.rtsClearAt = r.u64(); c.brk = r.boolean();
        c.rxEn = r.boolean(); c.rxActive = r.boolean(); c.rxByte = r.u8(); c.rxEnd = r.u64();
        c.rxNextAt = r.u64(); c.held = r.boolean(); c.heldEntry.data = r.u8();
        c.heldEntry.flags = r.u8();
        for (auto& e : c.fifo) { e.data = r.u8(); e.flags = r.u8(); }
        c.count = (int)r.u32(); c.accum = r.u8(); c.overrun = r.boolean();
        c.deltaBreak = r.boolean(); c.rtsNeg = r.boolean(); c.lastRead = r.u8();
        c.rxCount = r.u64();
        c.drivenValid = false;
    }
    acr_ = r.u8(); imr_ = r.u8(); opr_ = r.u8(); opcr_ = r.u8(); ctur_ = r.u8(); ctlr_ = r.u8();
    ipLast_ = r.u8(); ipDelta_ = r.u8(); isr7_ = r.boolean();
    brgTest_ = r.boolean(); test1x_ = r.boolean(); prevPoll_ = r.u64();
    ctrRunning_ = r.boolean(); ctrStart_ = r.u64(); ctrN_ = r.u32(); ctrPhase_ = r.u32();
    ctrLatched_ = r.boolean(); ctrClearAt_ = r.u64(); ctrFrozen_ = r.u16();
    ctrPending_ = r.boolean(); ctrPendingAt_ = r.u64(); ctrPendingN_ = r.u32();
    ctrPendingPhase_ = r.u32();
}

}  // namespace altair
