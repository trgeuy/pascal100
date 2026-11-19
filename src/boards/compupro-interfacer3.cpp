#include "boards/compupro-interfacer3.h"

#include "core/statefile.h"
#include "host/endpoint.h"
#include "host/stream.h"

#include <cstdio>
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

const char* const kBoardSel[4] = {"0-7", "8-15", "16-23", "24-31"};

} // namespace

void InterfacerThreeBoard::setResolver(EndpointResolver r) { g_resolver = std::move(r); }

InterfacerThreeBoard::InterfacerThreeBoard() {
    for (int i = 0; i < kUsers; ++i) {
        users_.push_back(std::make_unique<User>("user" + std::to_string(i)));
        users_.back()->uart.disconnect();  // -> NullStream: a dead line, never a null pointer
    }
}

InterfacerThreeBoard::~InterfacerThreeBoard() {
    if (clock_) clock_->cancel(wake_);  // a deadline firing into a freed board is a UAF
}

int InterfacerThreeBoard::userIndex(const std::string& unit) const {
    const std::string n = lowerAscii(unit);
    for (int i = 0; i < kUsers; ++i)
        if (n == "user" + std::to_string(i)) return i;
    return -1;
}

std::string InterfacerThreeBoard::defaultUnitTitle(const std::string& unit) const {
    const int i = userIndex(unit);
    if (i < 0) return Board::defaultUnitTitle(unit);
    return productName() + " port " + std::to_string(boardSel_ * 8 + i) + titleIdSuffix();
}

// ---------------------------------------------------------------------------
// Decode (U46/U47 address compare, U32 SEL*, U49 interrupt strobes, U35 chip enables)
// ---------------------------------------------------------------------------

// Relative 0-5 need SEL, and SEL needs USEL. Relative 6-7 (A1 = A2 = 1) are the user select
// port: a write is latched by every board at this base (snoop()), and claimed here by the
// board that is selected, so the bus delivers it once. A read there is not decoded.
bool InterfacerThreeBoard::decodes(const BusCycle& c) const {
    if (c.type != Cycle::IoRead && c.type != Cycle::IoWrite) return false;
    if (!inBlock(c.port())) return false;
    const int rel = c.port() & 7;
    if (rel >= 6) return c.type == Cycle::IoWrite && usel();
    return usel();
}

void InterfacerThreeBoard::setLatch(uint8_t v) {
    const bool was = usel();
    latch_         = (uint8_t)(v & 0x1F);  // U34 latches D0-D4
    if (usel() != was) decodeChanged();    // this board just joined or left the bus
}

// THE CLOCKED HALF: U34 is clocked by OUT0*, which needs only the address compare, sOUT
// and STROBE -- not SEL. So every board at this base takes the byte.
void InterfacerThreeBoard::snoop(const BusCycle& c) {
    if (c.type != Cycle::IoWrite || !inBlock(c.port())) return;
    if ((c.port() & 6) != 6) return;
    setLatch(c.data);
}

uint8_t InterfacerThreeBoard::read(const BusCycle& c) {
    const Clock& clk = clock_ ? *clock_ : deadCard();
    const int    rel = c.port() & 7;
    uint8_t      v   = 0xFF;
    if (rel <= 3) {
        holdReady((uint32_t)waits_);  // J17: USART accesses only
        Sig2651& u = users_[latch_ & 7]->uart;
        switch (rel) {
        case 0: v = u.readData(clk); break;     // RxRDY clears
        case 1: v = u.readStatus(clk); break;
        case 2: v = u.readMode(); break;
        case 3: v = u.readCommand(); break;
        }
    } else if (rel == 4 || rel == 5) {
        // IN2*/IN3*: the RDY lines through U37/U42 (Tx) and U51/U56 (Rx), whatever the masks.
        v = 0;
        for (int i = 0; i < kUsers; ++i) {
            Sig2651& u = users_[(size_t)i]->uart;
            u.poll(clk);
            if (rel == 4 ? u.txReady(clk) : u.rxReady()) v |= (uint8_t)(1u << i);
        }
    }
    refresh();  // a data read can drop RxINT
    return v;
}

void InterfacerThreeBoard::write(const BusCycle& c) {
    const Clock& clk = clock_ ? *clock_ : deadCard();
    const int    rel = c.port() & 7;
    if (rel <= 3) {
        holdReady((uint32_t)waits_);
        User& usr = *users_[latch_ & 7];
        Sig2651& u = usr.uart;
        switch (rel) {
        case 0: u.writeData(c.data, clk); break;
        case 1: break;  // SYN1/SYN2/DLE: synchronous mode only, not modeled
        case 2: u.writeMode(c.data); logUart(usr); break;
        case 3: u.writeCommand(c.data, clk); break;
        }
    } else if (rel == 4) {
        txMask_ = c.data;  // OUT2*: TxINTEN0-7
    } else if (rel == 5) {
        rxMask_ = c.data;  // OUT3*: RxINTEN0-7
    }
    // rel 6/7: the user select latch is taken in snoop(), on every board at this base.
    refresh();
}

void InterfacerThreeBoard::logUart(User& u) {
    for (auto& s : u.uart.drainLog()) log_.push_back(id + ":" + std::move(s));
}

// ---------------------------------------------------------------------------
// Interrupts (74LS38 U39/U40/U53/U54, J15/J16)
// ---------------------------------------------------------------------------

uint8_t InterfacerThreeBoard::assertsVi() const {
    if (!clock_) return 0;
    const Clock& clk = *clock_;
    uint8_t      m   = 0;
    for (int i = 0; i < kUsers; ++i) {
        const User& u = *users_[(size_t)i];
        if ((txMask_ >> i & 1) && u.uart.txReady(clk)) m |= viBit(u.txIrq);
        if ((rxMask_ >> i & 1) && u.uart.rxReady()) m |= viBit(u.rxIrq);
    }
    return m;
}

void InterfacerThreeBoard::refresh() {
    if (!clock_) return;
    for (auto& u : users_) u->uart.poll(*clock_);
    intChanged();
    clock_->cancel(wake_);
    wake_ = Clock::kNone;
    if (uint64_t next = nextEdge()) wake_ = clock_->at(next, [this] { refresh(); });
}

// The next T-state at which an enabled, jumpered source could move by itself. Zero = never.
uint64_t InterfacerThreeBoard::nextEdge() const {
    const Clock& clk  = clock_ ? *clock_ : deadCard();
    uint64_t     best = 0;
    auto         consider = [&](uint64_t when) {
        if (when <= clk.now()) return;  // strictly future, or the drain loop never stops
        if (!best || when < best) best = when;
    };
    for (int i = 0; i < kUsers; ++i) {
        const User& u = *users_[(size_t)i];
        if (u.txIrq != IrqJumper::None && (txMask_ >> i & 1)) consider(u.uart.txFreeAt());
        if (u.rxIrq != IrqJumper::None && (rxMask_ >> i & 1)) {
            if (u.uart.rxFrameActive()) consider(u.uart.rxDoneAt());
            else if (u.uart.rxWaiting()) consider(u.uart.rxNextAt());
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

// CLR (pRESET* with SLAVECLR*, or POC* through J18) clears U34 and the four mask latches, and
// drives each USART's RESET pin. The lines stay connected.
void InterfacerThreeBoard::reset(Reset) {
    const Clock& clk = clock_ ? *clock_ : deadCard();
    setLatch(0);
    txMask_ = rxMask_ = 0;
    for (auto& u : users_) {
        u->uart.powerOn(clk);
        logUart(*u);
    }
    refresh();
}

void InterfacerThreeBoard::power() { reset(Reset::PowerOn); }

void InterfacerThreeBoard::pump() {
    for (auto& u : users_) u->uart.pump();
    refresh();
}

void InterfacerThreeBoard::configChanged() {
    decodeChanged();
    for (auto& u : users_) {
        u->uart.programLine();
        logUart(*u);
    }
    refresh();
}

void InterfacerThreeBoard::serialize(StateWriter& w) const {
    Board::serialize(w);
    w.u8(latch_);
    w.u8(txMask_);
    w.u8(rxMask_);
    for (const auto& u : users_) u->uart.serialize(w);
}

void InterfacerThreeBoard::deserialize(StateReader& r) {
    Board::deserialize(r);
    latch_  = r.u8();
    txMask_ = r.u8();
    rxMask_ = r.u8();
    for (auto& u : users_) {
        u->uart.deserialize(r);
        u->uart.programLine();  // a restored format must reach a real serial port too
        logUart(*u);
    }
    decodeChanged();  // USEL may differ from before the restore
    refresh();
}

uint64_t InterfacerThreeBoard::rxBytes() const {
    uint64_t n = 0;
    for (const auto& u : users_) n += u->uart.rxBytes();
    return n;
}

std::vector<std::string> InterfacerThreeBoard::drainLog() {
    for (auto& u : users_) logUart(*u);
    auto out = std::move(log_);
    log_.clear();
    return out;
}

// ---------------------------------------------------------------------------
// Reflection
// ---------------------------------------------------------------------------

std::vector<Property> InterfacerThreeBoard::properties() {
    std::vector<Property> p;
    {
        Property x;
        x.name  = "port";
        x.help  = "Switch S1-2..6 (A7-A3): the base of the 8-port block, a multiple of 8. "
                  "CompuPro software uses 10H. S1-1 is `enabled`";
        x.kind  = Kind::Int;
        x.radix = 16;
        x.min   = 0;
        x.max   = 0xF8;
        x.get   = [this] { return Value::ofInt(base_); };
        x.set   = [this](const Value& v, std::string& err) {
            if (v.i() & 7) {
                err = "the Interfacer 3 decodes A7-A3 -- the port must be a multiple of 8";
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
        x.help    = "Switches S1-7/S1-8: which group of eight users this board answers, by "
                    "bits D3-D4 of the user select byte. Up to four boards share one block";
        x.kind    = Kind::Enum;
        x.choices = {kBoardSel[0], kBoardSel[1], kBoardSel[2], kBoardSel[3]};
        x.get     = [this] { return Value::ofStr(kBoardSel[boardSel_]); };
        x.set     = [this](const Value& v, std::string&) {
            for (int i = 0; i < 4; ++i)
                if (v.s() == kBoardSel[i]) boardSel_ = i;
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name    = "waits";
        x.help    = "J17: wait states on each USART access (relative ports 0-3). 0 = no shunt, "
                    "1 = A-C, 2 = B-C. The interrupt and user select ports never wait";
        x.kind    = Kind::Enum;
        x.choices = {"0", "1", "2"};
        x.get     = [this] { return Value::ofStr(std::to_string(waits_)); };
        x.set     = [this](const Value& v, std::string&) {
            waits_ = v.s()[0] - '0';
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name  = "select";
        x.help  = "The user select latch U34 (D0-D2 user, D3-D4 board), as last written";
        x.kind  = Kind::Int;
        x.radix = 16;
        x.get   = [this] { return Value::ofInt(latch_); };
        p.push_back(std::move(x));  // read-only
    }
    {
        Property x;
        x.name  = "tx_mask";
        x.help  = "The transmit interrupt mask (relative port 4), as last written";
        x.kind  = Kind::Int;
        x.radix = 16;
        x.get   = [this] { return Value::ofInt(txMask_); };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name  = "rx_mask";
        x.help  = "The receive interrupt mask (relative port 5), as last written";
        x.kind  = Kind::Int;
        x.radix = 16;
        x.get   = [this] { return Value::ofInt(rxMask_); };
        p.push_back(std::move(x));
    }
    return p;
}

std::vector<Property> InterfacerThreeBoard::boardUnitProperties(const std::string& unit) {
    const int i = userIndex(unit);
    if (i < 0) return {};
    User* u = users_[(size_t)i].get();
    std::vector<Property> p;
    // The 2651's `baud` and `connect`; its `interrupt` (a direct RxRDY jumper) is the SS1's
    // legacy path and does not exist on this board.
    for (Property& x : u->uart.properties(
             rebasingResolver(g_resolver, [this](const std::string& s) { return resolvePath(s); })))
        if (x.name != "interrupt") p.push_back(std::move(x));
    auto viOnly = [&](const char* name, const char* help, IrqJumper& j) {
        Property x = irqJumperProperty(name, help, j);
        std::erase(x.choices, std::string("int"));
        p.push_back(std::move(x));
    };
    viOnly("tx_int", "J15: where TxINT (TxRDY AND the user's tx_mask bit) is jumpered: none | "
           "vi0..vi7. CompuPro's MP/M 8-16 manual puts every TxINT on one line", u->txIrq);
    viOnly("rx_int", "J16: where RxINT (RxRDY AND the user's rx_mask bit) is jumpered: none | "
           "vi0..vi7", u->rxIrq);
    return p;
}

std::vector<UnitDef> InterfacerThreeBoard::units() const {
    std::vector<UnitDef> v;
    for (const auto& u : users_) v.push_back({u->uart.name(), UnitKind::Serial, u->uart.endpoint()});
    return v;
}

std::vector<MapEntry> InterfacerThreeBoard::ioMap() const {
    if (!enabled_) return {};
    const uint32_t b = base_;
    return {
        {b + 0, b + 0, "read/write", "selected 2651 -- receive (R) / transmit (W)"},
        {b + 1, b + 1, "read/write", "selected 2651 -- status (R) / SYN1/SYN2/DLE (W)"},
        {b + 2, b + 2, "read/write", "selected 2651 -- mode registers 1/2"},
        {b + 3, b + 3, "read/write", "selected 2651 -- command"},
        {b + 4, b + 4, "read/write", "TxRDY0-7 (R) / transmit interrupt mask (W)"},
        {b + 5, b + 5, "read/write", "RxRDY0-7 (R) / receive interrupt mask (W)"},
        {b + 6, b + 7, "write", "user select (D0-D2 user, D3-D4 board)"},
    };
}

static std::string noUnit(const std::string& unit) {
    return "interfacer3 has no unit '" + unit + "' -- its channels are 'user0' to 'user7'";
}

bool InterfacerThreeBoard::connect(const std::string& unit, const std::string& ep, std::string& err) {
    const int i = userIndex(unit);
    if (i < 0) {
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
    users_[(size_t)i]->uart.connect(std::move(s));
    refresh();
    return true;
}

bool InterfacerThreeBoard::connectStream(const std::string& unit, std::unique_ptr<ByteStream> s,
                                         std::string& err) {
    const int i = userIndex(unit);
    if (i < 0) {
        err = noUnit(unit);
        return false;
    }
    users_[(size_t)i]->uart.connect(std::move(s));
    refresh();
    return true;
}

bool InterfacerThreeBoard::disconnect(const std::string& unit, std::string& err) {
    const int i = userIndex(unit);
    if (i < 0) {
        err = noUnit(unit);
        return false;
    }
    users_[(size_t)i]->uart.disconnect();
    refresh();
    return true;
}

ByteStream* InterfacerThreeBoard::unitStream(const std::string& unit) {
    const int i = userIndex(unit);
    return i < 0 ? nullptr : &users_[(size_t)i]->uart.stream();
}

} // namespace altair
