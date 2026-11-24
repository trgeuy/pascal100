#include "boards/compupro-interfacer4.h"

#include "core/clock.h"
#include "core/statefile.h"
#include "host/endpoint.h"
#include "host/stream.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace altair {

namespace {

EndpointResolver g_resolver;

// A board on a bare bus with no machine has no clock; a USART with no clock reads as dead.
Clock& deadCard() {
    static Clock stopped;
    return stopped;
}

// S2-1..3: the four users this board answers.
const char* const kUsers[8] = {"0-3",   "4-7",   "8-11",  "12-15",
                               "16-19", "20-23", "24-27", "28-31"};

// Status-C (U12): D0 = not busy (BUSY through U17), D1 = 0, D2 = ACKNLG*, D3 = PE,
// D4/D5 = error (ERROR* through U17), D6 = SLCT, D7 = not busy (the same signal as D0).
constexpr uint8_t kNotBusy = 0x81;
constexpr uint8_t kAcknlg  = 0x04;
constexpr uint8_t kSlct    = 0x40;

// The printer's handshake after a data write: about 20 us in all. U10
// strobes the byte 1 us after the write, for 1 us; the printer answers with ACKNLG*, a
// 10 us low pulse (page 25), and TxI4 sets at its end. BUSY covers the whole window.
constexpr long long kHandshakePerSecond = 50000;  // 20 us
constexpr long long kAckPulsePerSecond  = 100000; // 10 us

} // namespace

void InterfacerFourBoard::setResolver(EndpointResolver r) { g_resolver = std::move(r); }

InterfacerFourBoard::InterfacerFourBoard() {
    for (int i = 1; i <= kSerial; ++i) {
        serial_.push_back(std::make_unique<Serial>("user" + std::to_string(i)));
        serial_.back()->uart.disconnect();  // -> NullStream: a dead line, never a null pointer
    }
    lpt_.stream = std::make_unique<NullStream>();
    par_.stream = std::make_unique<NullStream>();
}

InterfacerFourBoard::~InterfacerFourBoard() {
    if (clock_) {  // a deadline firing into a freed board is a UAF
        clock_->cancel(wake_);
        clock_->cancel(ack_);
    }
}

std::string InterfacerFourBoard::defaultUnitTitle(const std::string& unit) const {
    const std::string n = lowerAscii(unit);
    const int s = serialIndex(unit);
    int chip = s >= 0 ? s + 1 : (n == "centronics" || n == "parallel") ? 0 : -1;
    if (chip < 0) return Board::defaultUnitTitle(unit);
    // chipFor() swaps relative users 0 and 2 under J26 A-C/B-D; it is its own inverse, so it
    // also gives the relative user that reaches a chip.
    const int port = group_ * 8 + half_ * 4 + chipFor(chip);
    std::string t = productName() + " port " + std::to_string(port);
    if (chip == 0) t += " " + n;
    return t + titleIdSuffix();
}

int InterfacerFourBoard::serialIndex(const std::string& unit) const {
    const std::string n = lowerAscii(unit);
    for (int i = 0; i < kSerial; ++i)
        if (n == "user" + std::to_string(i + 1)) return i;
    return -1;
}

InterfacerFourBoard::Line* InterfacerFourBoard::lineFor(const std::string& unit) {
    const std::string n = lowerAscii(unit);
    if (n == "centronics") return &lpt_;
    if (n == "parallel") return &par_;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Decode (U22/U41 address compare, U23 SEL*/OUT7*, U25 chip enables, U44 interrupt strobes,
// U19/U24 data-out enables)
// ---------------------------------------------------------------------------

bool InterfacerFourBoard::decodes(const BusCycle& c) const {
    if (c.type != Cycle::IoRead && c.type != Cycle::IoWrite) return false;
    if (!inBlock(c.port())) return false;
    const int rel = c.port() & 7;
    if (rel <= 3) return groupMatch() && halfMatch();
    if (rel <= 5) {
        // SEL has no H/L term: both boards of the group read their nibble. A write is
        // latched by both through snoop(); the half the latch names claims it.
        if (c.type == Cycle::IoRead) return groupMatch();
        return groupMatch() && halfMatch();
    }
    // 6/7: the user select port. A read is not decoded (SEL* is off at A1 = A2 = 1).
    return c.type == Cycle::IoWrite && groupMatch() && halfMatch();
}

uint8_t InterfacerFourBoard::drivesBits(const BusCycle& c) const {
    const int rel = c.port() & 7;
    if (rel == 4 || rel == 5) return (uint8_t)(0x0F << shift());  // DOENL* or DOENH*
    // Status-P goes out through two sections of U9; D2-D7 are not driven.
    if (rel == 3 && selectedChip() == 0) return 0x03;
    return 0xFF;
}

void InterfacerFourBoard::setLatch(uint8_t v) {
    const bool g = groupMatch(), h = halfMatch();
    latch_ = (uint8_t)(v & 0x1F);  // U18 latches D0-D4
    if (g != groupMatch() || h != halfMatch()) decodeChanged();
}

// THE CLOCKED HALF. U18 is clocked by OUT7*, which needs the address compare, A1, A2, sOUT
// and STROBE -- not USEL. The mask latches U48/U52 are clocked by OUT4*/OUT5*, which need
// SEL, which has no H/L term: both boards of the group take the byte.
void InterfacerFourBoard::snoop(const BusCycle& c) {
    if (c.type != Cycle::IoWrite || !inBlock(c.port())) return;
    const int rel = c.port() & 7;
    if (rel >= 6) {
        setLatch(c.data);
        return;
    }
    if ((rel == 4 || rel == 5) && groupMatch()) {
        const uint8_t nib = (uint8_t)((c.data >> shift()) & 0x0F);
        (rel == 4 ? txMask_ : rxMask_) = nib;
        refresh();
    }
}

uint8_t InterfacerFourBoard::statusC() const {
    uint8_t s = kSlct;  // a line is always selected, never out of paper, never in error
    const uint64_t now = clock_ ? clock_->now() : 0;
    const bool handshake = ackEndAt_ && now < ackEndAt_;
    if (!handshake && lpt_.stream->writable()) s |= kNotBusy;
    if (!(handshake && now >= ackLowAt_)) s |= kAcknlg;  // ACKNLG* idles high
    return s;
}

uint8_t InterfacerFourBoard::txReadyBits() const {
    const Clock& clk = clock_ ? *clock_ : deadCard();
    uint8_t      v   = txi_ ? 1 : 0;
    for (int i = 0; i < kSerial; ++i)
        if (serial_[(size_t)i]->uart.txReady(clk)) v |= (uint8_t)(2u << i);
    return v;
}

uint8_t InterfacerFourBoard::rxReadyBits() const {
    uint8_t v = dav_ ? 1 : 0;
    for (int i = 0; i < kSerial; ++i)
        if (serial_[(size_t)i]->uart.rxReady()) v |= (uint8_t)(2u << i);
    return v;
}

uint8_t InterfacerFourBoard::read(const BusCycle& c) {
    const Clock& clk = clock_ ? *clock_ : deadCard();
    const int    rel = c.port() & 7;
    uint8_t      v   = 0xFF;
    if (rel <= 3) {
        holdReady((uint32_t)waits_);  // J6-J8: ports 0-3 only
        const int chip = selectedChip();
        if (chip == 0) {
            switch (rel) {
            case 0: v = sense_; break;     // DRD*: S1-3..10 on D7..D0
            case 1: v = statusC(); break;  // SRD*
            case 2:                        // MRD*: the input latch; clears DAV and RxI4
                v    = inLatch_;
                dav_ = false;
                takeParallelInput();       // the next byte may latch now
                break;
            case 3:                        // CRD*
                if (!dav_) ++emptyPolls_;  // no parallel byte: the guest is waiting
                v = (uint8_t)((dav_ ? 1 : 0) | (dntkn_ ? 2 : 0));
                break;
            }
        } else {
            Sig2651& u = serial_[(size_t)chip - 1]->uart;
            switch (rel) {
            case 0: v = u.readData(clk); break;
            case 1: v = u.readStatus(clk); break;
            case 2: v = u.readMode(); break;
            case 3: v = u.readCommand(); break;
            }
        }
    } else if (rel == 4 || rel == 5) {
        for (auto& s : serial_) s->uart.poll(clk);
        const uint8_t bits = rel == 4 ? txReadyBits() : rxReadyBits();
        if (rel == 5 && bits == 0) ++emptyPolls_;  // no user has a byte: the guest is waiting
        v = (uint8_t)(bits << shift());
    }
    refresh();
    return v;
}

void InterfacerFourBoard::write(const BusCycle& c) {
    const Clock& clk = clock_ ? *clock_ : deadCard();
    const int    rel = c.port() & 7;
    if (rel > 3) return;  // 4/5 and 6/7 are latched in snoop(), on every board that takes them
    holdReady((uint32_t)waits_);
    const int chip = selectedChip();
    if (chip == 0) {
        switch (rel) {
        case 0: printerWrite(c.data); break;               // DWR*
        case 1: control_ = (uint8_t)(c.data & 0x0F); break; // SWR*: U13
        case 2:                                            // MWR*: U7, sets U8b
            par_.stream->writeByte(c.data);
            dntkn_ = !par_.stream->writable();  // a stream takes the byte as it is written
            break;
        case 3: break;  // CWR* goes nowhere
        }
    } else {
        Serial&  s = *serial_[(size_t)chip - 1];
        Sig2651& u = s.uart;
        switch (rel) {
        case 0: u.writeData(c.data, clk); break;
        case 1: break;  // SYN1/SYN2/DLE: synchronous mode only, not modeled
        case 2: u.writeMode(c.data); logUart(s); break;
        case 3: u.writeCommand(c.data, clk); break;
        }
    }
    refresh();
}

void InterfacerFourBoard::logUart(Serial& s) {
    for (auto& m : s.uart.drainLog()) log_.push_back(id + ":" + std::move(m));
}

// ---------------------------------------------------------------------------
// The Centronics port (U10, U11, U12, U16b) and the Universal Parallel port (U6-U8)
// ---------------------------------------------------------------------------

void InterfacerFourBoard::printerWrite(uint8_t v) {
    lpt_.stream->writeByte(v);
    txi_ = false;  // DWR* clears U16b
    if (!clock_) return;
    clock_->cancel(ack_);
    const uint64_t now  = clock_->now();
    const uint64_t full = clock_->tStatesPer(kHandshakePerSecond);
    const uint64_t low  = clock_->tStatesPer(kAckPulsePerSecond);
    ackEndAt_ = now + (full ? full : 2);
    ackLowAt_ = ackEndAt_ - (low && low < full ? low : 1);
    ack_      = clock_->at(ackEndAt_, [this] { onAcknowledge(); });
}

// The rising edge of ACKNLG* (J5 TOP) -- and, in this model, the end of BUSY (J5 BOTTOM) at
// the same moment -- clocks a 1 into U16b.
void InterfacerFourBoard::onAcknowledge() {
    ack_      = Clock::kNone;
    ackEndAt_ = ackLowAt_ = 0;
    txi_      = true;
    refresh();
}

// STROBE on CONN1 latches the input into U6 and sets U8a. A stream delivers a byte as a
// strobe; the next one waits until the guest has read this one.
void InterfacerFourBoard::takeParallelInput() {
    if (dav_ || !par_.stream->readable()) return;
    inLatch_ = par_.stream->readByte();
    dav_     = true;
    ++parRxBytes_;
}

// ---------------------------------------------------------------------------
// Interrupts (74LS38 U47/U51, JS5/JS6)
// ---------------------------------------------------------------------------

uint8_t InterfacerFourBoard::assertsVi() const {
    if (!clock_) return 0;
    const Clock& clk = *clock_;
    uint8_t      m   = 0;
    if ((txMask_ & 1) && txi_) m |= viBit(lpt_.irq);
    if ((rxMask_ & 1) && dav_) m |= viBit(par_.irq);
    for (int i = 0; i < kSerial; ++i) {
        const Serial& s = *serial_[(size_t)i];
        if ((txMask_ >> (i + 1) & 1) && s.uart.txReady(clk)) m |= viBit(s.txIrq);
        if ((rxMask_ >> (i + 1) & 1) && s.uart.rxReady()) m |= viBit(s.rxIrq);
    }
    return m;
}

void InterfacerFourBoard::refresh() {
    if (!clock_) return;
    for (auto& s : serial_) s->uart.poll(*clock_);
    intChanged();
    clock_->cancel(wake_);
    wake_ = Clock::kNone;
    if (uint64_t next = nextEdge()) wake_ = clock_->at(next, [this] { refresh(); });
}

// The next T-state at which an enabled, jumpered USART source could move by itself. Zero =
// never. (The printer acknowledge has its own deadline.)
uint64_t InterfacerFourBoard::nextEdge() const {
    const Clock& clk  = clock_ ? *clock_ : deadCard();
    uint64_t     best = 0;
    auto         consider = [&](uint64_t when) {
        if (when <= clk.now()) return;  // strictly future, or the drain loop never stops
        if (!best || when < best) best = when;
    };
    for (int i = 0; i < kSerial; ++i) {
        const Serial& s = *serial_[(size_t)i];
        if (s.txIrq != IrqJumper::None && (txMask_ >> (i + 1) & 1)) consider(s.uart.txFreeAt());
        if (s.rxIrq != IrqJumper::None && (rxMask_ >> (i + 1) & 1)) {
            if (s.uart.rxFrameActive()) consider(s.uart.rxDoneAt());
            else if (s.uart.rxWaiting()) consider(s.uart.rxNextAt());
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

// CLR (pRESET*; J9 adds SLVCLR* or POC) clears U18, the mask latches, Control-C (U13, so the
// LED goes off), the input flop U8a, and each USART. It does NOT clear U16b (TxI4): its only
// clear is DWR*. The lines stay connected.
void InterfacerFourBoard::reset(Reset) {
    const Clock& clk = clock_ ? *clock_ : deadCard();
    setLatch(0);
    txMask_ = rxMask_ = 0;
    control_ = 0;
    dav_     = false;
    for (auto& s : serial_) {
        s->uart.powerOn(clk);
        logUart(*s);
    }
    lpt_.stream->flush();
    refresh();
}

// At power-up U16b and U8b hold whatever they come up with; the drawing does not say. This
// model starts them clear (docs/boards/compupro-interfacer4.md, Limitations).
void InterfacerFourBoard::power() {
    if (clock_) clock_->cancel(ack_);
    ack_      = Clock::kNone;
    ackEndAt_ = ackLowAt_ = 0;
    txi_      = false;
    dntkn_    = false;
    inLatch_  = 0xFF;
    reset(Reset::PowerOn);
}

void InterfacerFourBoard::pump() {
    for (auto& s : serial_) s->uart.pump();
    lpt_.stream->pump();
    lpt_.stream->flush();
    par_.stream->pump();
    par_.stream->flush();
    if (dntkn_ && par_.stream->writable()) dntkn_ = false;
    takeParallelInput();
    refresh();
}

void InterfacerFourBoard::configChanged() {
    decodeChanged();
    for (auto& s : serial_) {
        s->uart.programLine();
        logUart(*s);
    }
    refresh();
}

void InterfacerFourBoard::serialize(StateWriter& w) const {
    Board::serialize(w);
    w.u8(latch_);
    w.u8(txMask_);
    w.u8(rxMask_);
    w.u8(control_);
    w.boolean(txi_ || ack_ != Clock::kNone);  // a handshake in flight completes on restore
    w.boolean(dav_);
    w.boolean(dntkn_);
    w.u8(inLatch_);
    for (const auto& s : serial_) s->uart.serialize(w);
}

void InterfacerFourBoard::deserialize(StateReader& r) {
    Board::deserialize(r);
    latch_   = r.u8();
    txMask_  = r.u8();
    rxMask_  = r.u8();
    control_ = r.u8();
    if (clock_) clock_->cancel(ack_);
    ack_      = Clock::kNone;
    ackEndAt_ = ackLowAt_ = 0;
    txi_      = r.boolean();
    dav_      = r.boolean();
    dntkn_    = r.boolean();
    inLatch_  = r.u8();
    for (auto& s : serial_) {
        s->uart.deserialize(r);
        s->uart.programLine();
        logUart(*s);
    }
    decodeChanged();
    refresh();
}

uint64_t InterfacerFourBoard::emptyPolls() const {
    uint64_t n = emptyPolls_;
    for (const auto& s : serial_) n += s->uart.emptyPolls();
    return n;
}

uint64_t InterfacerFourBoard::rxBytes() const {
    uint64_t n = parRxBytes_;
    for (const auto& s : serial_) n += s->uart.rxBytes();
    return n;
}

std::vector<std::string> InterfacerFourBoard::drainLog() {
    for (auto& s : serial_) logUart(*s);
    auto out = std::move(log_);
    log_.clear();
    return out;
}

// ---------------------------------------------------------------------------
// Reflection
// ---------------------------------------------------------------------------

namespace {
Property readOnlyHex(const char* name, const char* help, std::function<int()> get) {
    Property x;
    x.name  = name;
    x.help  = help;
    x.kind  = Kind::Int;
    x.radix = 16;
    x.get   = [get] { return Value::ofInt(get()); };
    return x;
}
} // namespace

std::vector<Property> InterfacerFourBoard::properties() {
    std::vector<Property> p;
    {
        Property x;
        x.name  = "port";
        x.help  = "Switch S2-5..9 (A7-A3): the base of the 8-port block, a multiple of 8. "
                  "CompuPro software uses 10H. S2-4 is `enabled`";
        x.kind  = Kind::Int;
        x.radix = 16;
        x.min   = 0;
        x.max   = 0xF8;
        x.get   = [this] { return Value::ofInt(base_); };
        x.set   = [this](const Value& v, std::string& err) {
            if (v.i() & 7) {
                err = "the Interfacer 4 decodes A7-A3 -- the port must be a multiple of 8";
                return false;
            }
            base_ = (uint8_t)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name    = "users";
        x.help    = "Switches S2-1/S2-2 (the group of eight, D3-D4 of the user select byte) and "
                    "S2-3 (the half, D2). Two boards make a group. S3 follows S2-3, as page 10 "
                    "of the manual requires. CompuPro software uses 4-7";
        x.kind    = Kind::Enum;
        x.choices = {kUsers[0], kUsers[1], kUsers[2], kUsers[3],
                     kUsers[4], kUsers[5], kUsers[6], kUsers[7]};
        x.get     = [this] { return Value::ofStr(kUsers[group_ * 2 + half_]); };
        x.set     = [this](const Value& v, std::string&) {
            for (int i = 0; i < 8; ++i)
                if (v.s() == kUsers[i]) {
                    group_ = i >> 1;
                    half_  = i & 1;
                }
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name    = "waits";
        x.help    = "J6/J7/J8: wait states on each access of ports 0-3. The interrupt and user "
                    "select ports never wait. Set every board at the base the same";
        x.kind    = Kind::Enum;
        x.choices = {"0", "1", "2", "3"};
        x.get     = [this] { return Value::ofStr(std::to_string(waits_)); };
        x.set     = [this](const Value& v, std::string&) {
            waits_ = v.s()[0] - '0';
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name    = "j26";
        x.help    = "J26: ab-cd = the parallel ports are relative user 0 (standard, and what "
                    "MP/M 8-16 needs); ac-bd = they are relative user 2 and the middle serial "
                    "channel is user 0. The status and mask bits do NOT move";
        x.kind    = Kind::Enum;
        x.choices = {"ab-cd", "ac-bd"};
        x.get     = [this] { return Value::ofStr(swap_ ? "ac-bd" : "ab-cd"); };
        x.set     = [this](const Value& v, std::string&) {
            swap_ = v.s() == "ac-bd";
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name  = "sense";
        x.help  = "Switch S1-3..10, read at relative port 0 of the parallel user: bit 7 = "
                  "S1-3 ... bit 0 = S1-10. A switch ON reads 0. The manual's set-up has all OFF";
        x.kind  = Kind::Int;
        x.radix = 16;
        x.min   = 0;
        x.max   = 0xFF;
        x.get   = [this] { return Value::ofInt(sense_); };
        x.set   = [this](const Value& v, std::string&) {
            sense_ = (uint8_t)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    p.push_back(readOnlyHex("select", "The user select latch U18 (D0-D1 user, D2 half, D3-D4 "
                            "group), as last written", [this] { return (int)latch_; }));
    p.push_back(readOnlyHex("tx_mask", "This board's transmit interrupt mask (bit 0 the "
                            "Centronics port, bits 1-3 users 1-3)", [this] { return (int)txMask_; }));
    p.push_back(readOnlyHex("rx_mask", "This board's receive interrupt mask (bit 0 the "
                            "Universal Parallel port, bits 1-3 users 1-3)",
                            [this] { return (int)rxMask_; }));
    p.push_back(readOnlyHex("control", "Control-C (U13): D0 AFX, D1 INIT, D2 SLCTIN, D3 LED",
                            [this] { return (int)control_; }));
    return p;
}

std::vector<Property> InterfacerFourBoard::boardUnitProperties(const std::string& unit) {
    std::vector<Property> p;
    auto viOnly = [&](const char* name, const char* help, IrqJumper& j) {
        Property x = irqJumperProperty(name, help, j);
        std::erase(x.choices, std::string("int"));
        p.push_back(std::move(x));
    };
    if (Line* l = lineFor(unit)) {
        Property x;
        x.name = "connect";
        x.help = l == &lpt_ ? "The printer on the Centronics port: out:FILE, printer:QUEUE, a "
                              "socket, ... (CONNECT sets this)"
                            : "The device on the Universal Parallel port (CONNECT sets this)";
        x.kind = Kind::Str;
        x.get  = [l] { return Value::ofStr(l->spec); };
        x.set  = [this, u = lowerAscii(unit)](const Value& v, std::string& err) {
            return connect(u, v.s(), err);
        };
        p.push_back(std::move(x));
        if (l == &lpt_)
            viOnly("tx_int", "JS5: where TxINT of the parallel user (the printer's acknowledge "
                   "AND tx_mask bit 0) is jumpered: none | vi0..vi7", l->irq);
        else
            viOnly("rx_int", "JS6: where RxINT of the parallel user (input latched AND rx_mask "
                   "bit 0) is jumpered: none | vi0..vi7", l->irq);
        return p;
    }
    const int i = serialIndex(unit);
    if (i < 0) return {};
    Serial* s = serial_[(size_t)i].get();
    for (Property& x : s->uart.properties(
             rebasingResolver(g_resolver, [this](const std::string& q) { return resolvePath(q); })))
        if (x.name != "interrupt") p.push_back(std::move(x));
    viOnly("tx_int", "JS5: where TxINT (TxRDY AND the user's tx_mask bit) is jumpered: none | "
           "vi0..vi7. CompuPro's MP/M 8-16 manual puts every TxINT on one line", s->txIrq);
    viOnly("rx_int", "JS6: where RxINT (RxRDY AND the user's rx_mask bit) is jumpered: none | "
           "vi0..vi7", s->rxIrq);
    return p;
}

std::vector<UnitDef> InterfacerFourBoard::units() const {
    std::vector<UnitDef> v;
    v.push_back({"centronics", UnitKind::Serial, lpt_.spec});
    v.push_back({"parallel", UnitKind::Serial, par_.spec});
    for (const auto& s : serial_) v.push_back({s->uart.name(), UnitKind::Serial, s->uart.endpoint()});
    return v;
}

std::vector<MapEntry> InterfacerFourBoard::ioMap() const {
    if (!enabled_) return {};
    const uint32_t b = base_;
    return {
        {b + 0, b + 0, "read/write", "selected user -- 2651 data / Centronics data (W), sense switch (R)"},
        {b + 1, b + 1, "read/write", "selected user -- 2651 status / Centronics Status-C (R), Control-C (W)"},
        {b + 2, b + 2, "read/write", "selected user -- 2651 mode / Universal Parallel data"},
        {b + 3, b + 3, "read/write", "selected user -- 2651 command / Universal Parallel Status-P (R)"},
        {b + 4, b + 4, "read/write", "Tx ready nibble (R) / transmit interrupt mask nibble (W)"},
        {b + 5, b + 5, "read/write", "Rx ready nibble (R) / receive interrupt mask nibble (W)"},
        {b + 6, b + 7, "write", "user select (D0-D1 user, D2 half, D3-D4 group)"},
    };
}

static std::string noUnit(const std::string& unit) {
    return "interfacer4 has no unit '" + unit +
           "' -- its units are 'centronics', 'parallel' and 'user1' to 'user3'";
}

bool InterfacerFourBoard::attach(Line& l, std::unique_ptr<ByteStream> s, const std::string& spec) {
    l.stream = std::move(s);
    l.spec   = spec;
    if (&l == &par_) takeParallelInput();
    refresh();
    return true;
}

bool InterfacerFourBoard::connect(const std::string& unit, const std::string& ep, std::string& err) {
    Line*     l = lineFor(unit);
    const int i = serialIndex(unit);
    if (!l && i < 0) {
        err = noUnit(unit);
        return false;
    }
    if (!g_resolver) {
        err = "no endpoint resolver installed";
        return false;
    }
    std::vector<std::string> paths;
    auto s = openEndpoint(
        g_resolver, ep, [this](const std::string& pth) { return resolvePath(pth); }, err,
        &paths);
    if (!s) {
        for (const std::string& pth : paths) err += pathNote(pth);
        return false;
    }
    if (l) return attach(*l, std::move(s), ep);
    serial_[(size_t)i]->uart.connect(std::move(s));
    refresh();
    return true;
}

bool InterfacerFourBoard::connectStream(const std::string& unit, std::unique_ptr<ByteStream> s,
                                        std::string& err) {
    if (Line* l = lineFor(unit)) {
        const std::string spec = s->describe();  // a pre-built line names itself
        return attach(*l, std::move(s), spec);
    }
    const int i = serialIndex(unit);
    if (i < 0) {
        err = noUnit(unit);
        return false;
    }
    serial_[(size_t)i]->uart.connect(std::move(s));
    refresh();
    return true;
}

bool InterfacerFourBoard::disconnect(const std::string& unit, std::string& err) {
    if (Line* l = lineFor(unit)) {
        l->stream->flush();
        return attach(*l, std::make_unique<NullStream>(), "null");
    }
    const int i = serialIndex(unit);
    if (i < 0) {
        err = noUnit(unit);
        return false;
    }
    serial_[(size_t)i]->uart.disconnect();
    refresh();
    return true;
}

ByteStream* InterfacerFourBoard::unitStream(const std::string& unit) {
    if (Line* l = lineFor(unit)) return l->stream.get();
    const int i = serialIndex(unit);
    return i < 0 ? nullptr : &serial_[(size_t)i]->uart.stream();
}

} // namespace altair
