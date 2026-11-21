#include "boards/compupro-pcvideo.h"

#include "core/bus.h"
#include "core/hex.h"
#include "core/roms.h"
#include "core/statefile.h"
#include "core/value.h"
#include "host/endpoint.h"
#include "host/stream.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <utility>

namespace altair {

namespace {

Display*         g_display = nullptr;
EndpointResolver g_resolver;
std::set<PcVideoBoard*> g_live;   // every board alive, for forOwner()

// The 16 colors of Table 10, as an RGBI monitor shows them. THE LEVELS ARE A MODEL DECISION,
// not the manual's: 2/3 for each color line, 1/3 more with intensity, and the green of color 6
// at half, so that it is the brown Table 10 names (DESIGN.md 7.4.1).
const Color kPalette[17] = {
    {0x00, 0x00, 0x00, 255}, {0x00, 0x00, 0xAA, 255}, {0x00, 0xAA, 0x00, 255},
    {0x00, 0xAA, 0xAA, 255}, {0xAA, 0x00, 0x00, 255}, {0xAA, 0x00, 0xAA, 255},
    {0xAA, 0x55, 0x00, 255}, {0xAA, 0xAA, 0xAA, 255}, {0x55, 0x55, 0x55, 255},
    {0x55, 0x55, 0xFF, 255}, {0x55, 0xFF, 0x55, 255}, {0x55, 0xFF, 0xFF, 255},
    {0xFF, 0x55, 0x55, 255}, {0xFF, 0x55, 0xFF, 255}, {0xFF, 0xFF, 0x55, 255},
    {0xFF, 0xFF, 0xFF, 255},
    {0xB4, 0xB4, 0xB4, 255},  // 16: the title bar, the terminal windows' neutral gray
};

// ---------------------------------------------------------------------------
// ASCII -> IBM PC keyboard scan codes, from CompuPro's own tables in PCKEY.DVR (KEY_TABLE
// and SHIFT_TABLE, scan codes 1-57). A control character is Ctrl and its letter.
// ---------------------------------------------------------------------------
constexpr uint8_t kShiftKey = 42;  // left shift
constexpr uint8_t kCtrlKey  = 29;
constexpr uint8_t kModShift = 1;
constexpr uint8_t kModCtrl  = 2;

struct Key {
    uint8_t code = 0;
    uint8_t mods = 0;
};

struct KeyTable {
    Key k[128];
    KeyTable() {
        auto row = [this](uint8_t first, const char* plain, const char* shifted) {
            for (int i = 0; plain[i]; ++i) {
                const uint8_t code = (uint8_t)(first + i);
                if (k[(uint8_t)plain[i]].code == 0) k[(uint8_t)plain[i]] = {code, 0};
                if (shifted && k[(uint8_t)shifted[i]].code == 0)
                    k[(uint8_t)shifted[i]] = {code, kModShift};
            }
        };
        k[0x1B] = {1, 0};    // Esc
        k[0x08] = {14, 0};   // backspace
        k[0x09] = {15, 0};   // tab
        k[0x0D] = {28, 0};   // return
        k[0x7F] = {83, 0};   // Del, the keypad's "." key
        row(2, "1234567890-=", "!@#$%^&*()_+");
        row(16, "qwertyuiop[]", "QWERTYUIOP{}");
        row(30, "asdfghjkl;'`", "ASDFGHJKL:\"~");
        row(43, "\\zxcvbnm,./", "|ZXCVBNM<>?");
        k[(uint8_t)' '] = {57, 0};
        if (k[(uint8_t)'*'].code == 0) k[(uint8_t)'*'] = {55, 0};
        // Ctrl-A to Ctrl-Z, and Ctrl-\ ] ^ _, where the character has no key of its own.
        for (int c = 1; c <= 0x1F; ++c) {
            if (k[c].code) continue;
            const char base = c <= 26 ? (char)('a' + c - 1) : "\\]^_"[c - 27];
            const Key& b = k[(uint8_t)base];
            if (b.code) k[c] = {b.code, (uint8_t)(b.mods | kModCtrl)};
        }
    }
};

const KeyTable& keyTable() {
    static const KeyTable t;
    return t;
}

} // namespace

void PcVideoBoard::setDisplay(Display* d) { g_display = d; }
Display* PcVideoBoard::display() { return g_display; }
void PcVideoBoard::setResolver(EndpointResolver r) { g_resolver = std::move(r); }

PcVideoBoard::PcVideoBoard() : kbd_(std::make_unique<NullStream>()) {
    std::string err;
    loadFont(font_, err);
    g_live.insert(this);
    if (g_display) g_display->claimRawKeys(this);
}

PcVideoBoard::~PcVideoBoard() {
    if (clock_) {
        clock_->cancel(keyEvt_);
        clock_->cancel(penEvt_);
    }
    if (g_display) g_display->releaseRawKeys(this);
    g_live.erase(this);
}

PcVideoBoard* PcVideoBoard::forOwner(const void* owner) {
    // Compared as an address only; followed only when it is one of ours.
    auto it = g_live.find(static_cast<PcVideoBoard*>(const_cast<void*>(owner)));
    return it == g_live.end() ? nullptr : *it;
}

// ---------------------------------------------------------------------------
// TIME: host T-states -> 14.31818 MHz master clocks -> the 6845's character clocks.
// ---------------------------------------------------------------------------
void PcVideoBoard::catchUp() {
    if (!clock_) return;
    const uint64_t now = clock_->now();
    if (now <= anchorT_) {
        anchorT_ = now;
        return;
    }
    const uint64_t hz = (uint64_t)clock_->hz();
    const uint64_t dt = now - anchorT_;
    anchorT_ = now;
    const uint64_t part = (dt % hz) * (uint64_t)kMasterHz + rem_;
    const uint64_t mclk = (dt / hz) * (uint64_t)kMasterHz + part / hz;
    rem_ = part % hz;
    const uint64_t per = (uint64_t)mclkPerChar();
    const uint64_t total = mclkPhase_ + mclk;
    crtc_.advance(total / per);
    mclkPhase_ = total % per;
}

// PRDY (pp. 44-45, 48, 51-52). The request waits for the next transfer window of the
// character clock: state 3 of an 8-clock cycle, states 4-11 of a 16-clock cycle. The transfer
// starts one master clock later and takes 2 master clocks, or 8 for the 6845.
void PcVideoBoard::waitFor(bool crtcPort) {
    if (!clock_) return;
    const uint64_t per = (uint64_t)mclkPerChar();
    const uint64_t ph = mclkPhase_;
    uint64_t w;
    if (per == 8)
        w = (3 + 8 - ph) % 8;
    else
        w = (ph >= 4 && ph <= 11) ? 0 : (ph < 4 ? 4 - ph : 16 - ph + 4);
    const uint64_t mclk = w + 1 + (crtcPort ? 8 : 2);
    const uint64_t hz = (uint64_t)clock_->hz();
    holdReady((uint32_t)((mclk * hz + (uint64_t)kMasterHz - 1) / (uint64_t)kMasterHz));
}

// ---------------------------------------------------------------------------
// THE BUS SIDE (Tables 3, 11-14; logic sheets 4-8).
// ---------------------------------------------------------------------------
bool PcVideoBoard::memHit(const BusCycle& c) const {
    if (!psel_) return false;
    const uint32_t a = c.phys() ^ (flip_ ? 0x8000u : 0u);   // port 1 inverts A15's compare
    return (a & 0xFFC000u) == memory_;
}

int PcVideoBoard::ioReg(const BusCycle& c) const {
    const uint16_t a = (uint16_t)(c.addr ^ (flip_ ? 0x0060u : 0u));  // ...and A5-A6's
    if ((a & 0xFFF0u) != ports_) return -1;
    return a & 0x0F;
}

bool PcVideoBoard::decodes(const BusCycle& c) const {
    if (!enabled_) return false;
    switch (c.type) {
        case Cycle::MemRead:  return !wom_ && memHit(c);   // S1-1 OFF: reads are not ours
        case Cycle::MemWrite: return memHit(c);
        case Cycle::IoRead:
        case Cycle::IoWrite: {
            const int r = ioReg(c);
            if (r < 0) return false;
            if (!psel_) return r == 0 && c.type == Cycle::IoWrite;  // only SETPSEL works
            return true;
        }
        default: return false;
    }
}

// The port compare reads A4-A15 (S2-1..8 = A11-A4, A15-A12 = 0). Both blocks the address flip
// can reach are named, so the answer does not change when the guest writes port 1.
bool PcVideoBoard::decodesIoHigh(uint8_t port) const {
    const uint8_t hi = (uint8_t)(port & 0xF0);
    if (hi == (ports_ & 0xF0)) return true;
    return flipOk_ && hi == ((ports_ ^ 0x60) & 0xF0);
}

bool PcVideoBoard::assertsPhantom(const BusCycle& c) const {
    return enabled_ && phantom_ && (c.type == Cycle::MemRead || c.type == Cycle::MemWrite) &&
           decodes(c);
}

bool PcVideoBoard::answersSixtn(const BusCycle& c) const {
    return enabled_ && !wom_ && !(c.addr & 1) &&
           (c.type == Cycle::MemRead || c.type == Cycle::MemWrite) && memHit(c);
}

uint8_t PcVideoBoard::status() const {
    uint8_t v = 0xF0;                        // D4-D7: the undriven internal bus (sheet 6)
    if (!crtc_.de()) v |= 0x01;              // -DLYDSP: 1 while the beam is blanked
    if (ready_) v |= 0x02;                   // KBDSTB
    if (!(lightpen_ && penOn_)) v |= 0x04;   // -LPENSW: 0 while the switch is closed
    if (crtc_.vsync()) v |= 0x08;            // DLYVSYNC
    return v;
}

uint8_t PcVideoBoard::read(const BusCycle& c) {
    catchUp();
    if (c.type == Cycle::MemRead) {
        waitFor(false);
        return ram_[offset(c)];
    }
    const int r = ioReg(c);
    waitFor(r == 4 || r == 5);
    switch (r) {
        case 5: {
            uint8_t v = 0xFF;                // a write-only register: the bus floats
            crtc_.readData(v);
            return v;
        }
        case 10: return status();
        case 12: return kbData_;
        default: return 0xFF;                // no strobe behind it (sheet 4)
    }
}

void PcVideoBoard::write(const BusCycle& c) {
    catchUp();
    if (c.type == Cycle::MemWrite) {
        waitFor(false);
        ram_[offset(c)] = c.data;
        dirty_ = true;
        return;
    }
    const int r = ioReg(c);
    waitFor(r == 4 || r == 5);
    bool timing = false;                     // a change that moves the light pen's moment
    switch (r) {
        case 0:
            psel_ = (c.data >> select_) & 1;
            decodeChanged();
            break;
        case 1:
            if (flipOk_) {
                flip_ = c.data & 1;
                decodeChanged();
            }
            break;
        case 4: crtc_.selectRegister(c.data); break;
        case 5:
            crtc_.writeData(c.data);
            dirty_ = timing = true;
            break;
        case 8:
            mode_ = c.data & 0x3F;
            dirty_ = timing = true;
            break;
        case 9:
            color_ = c.data & 0x3F;
            dirty_ = true;
            break;
        case 11: clearKeyboard(c.data); break;
        case 12:
            ready_ = true;
            intChanged();
            break;
        default: break;
    }
    if (timing && penEvt_ != Clock::kNone && clock_) {
        clock_->cancel(penEvt_);
        penEvt_ = Clock::kNone;
        armPen();
    }
}

void PcVideoBoard::read16(const BusCycle& c, uint8_t& even, uint8_t& odd) {
    catchUp();
    waitFor(false);
    even = ram_[offset(c)];
    odd  = ram_[(offset(c) + 1) & 0x3FFF];
}

void PcVideoBoard::write16(const BusCycle& c) {
    catchUp();
    waitFor(false);
    ram_[offset(c)] = c.data;
    ram_[(offset(c) + 1) & 0x3FFF] = c.dataHi;   // DI0-DI7 (U44, sheet 7)
    dirty_ = true;
}

bool PcVideoBoard::peekX(uint32_t phys, uint8_t& out) const {
    if (!enabled_ || !psel_ || wom_) return false;
    if (((phys ^ (flip_ ? 0x8000u : 0u)) & 0xFFC000u) != memory_) return false;
    out = ram_[phys & 0x3FFF];
    return true;
}

// ---------------------------------------------------------------------------
// THE KEYBOARD (pp. 22, 68-70). One code at a time: the board holds the keyboard's data line
// until the guest writes port 11. The keyboard sends its next code 1 ms of emulated time
// later -- a model decision, about one PC keyboard character (DESIGN.md 7.4.1).
// ---------------------------------------------------------------------------
void PcVideoBoard::clearKeyboard(uint8_t data) {
    ready_  = false;
    kbData_ = 0;                             // "a zero is forced into the data register"
    kbHeld_ = data & 0x80;
    if (clock_) nextKeyAt_ = clock_->now() + (uint64_t)clock_->hz() / 1000;
    intChanged();
    armKey();
}

void PcVideoBoard::deliverKey() {
    if (!clock_ || ready_ || kbHeld_ || kq_.empty()) return;
    if (clock_->now() < nextKeyAt_) return;
    kbData_ = kq_.front();
    kq_.pop_front();
    ready_ = true;
    intChanged();
}

void PcVideoBoard::armKey() {
    if (!clock_ || keyEvt_ != Clock::kNone || ready_ || kbHeld_ || kq_.empty()) return;
    const uint64_t when = std::max(nextKeyAt_, clock_->now());
    keyEvt_ = clock_->at(when, [this] {
        keyEvt_ = Clock::kNone;
        deliverKey();
        armKey();
    });
}

void PcVideoBoard::keyCodes(const uint8_t* codes, size_t n) {
    kq_.insert(kq_.end(), codes, codes + n);
    deliverKey();
    armKey();
}

// USB HID usage (keyboard page 07h) -> the 83-key PC/XT keyboard's scan code. CompuPro's
// driver takes codes 1-83 and no E0 prefix (PCKEY.DVR: "CMP AL,83"), so a key the XT did not
// have goes to the XT key that did its job: the arrows, Home, End, PgUp, PgDn, Insert and
// Delete to the keypad, right Ctrl and Alt to the only Ctrl and Alt, Print Screen to PrtSc *.
// F11, F12, Pause and the GUI keys have no code.
uint8_t PcVideoBoard::scanCodeFor(uint16_t u) {
    static const uint8_t letters[26] = {30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38, 50,
                                        49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44};
    if (u >= 0x04 && u <= 0x1D) return letters[u - 0x04];        // A-Z
    if (u >= 0x1E && u <= 0x27) return (uint8_t)(2 + (u - 0x1E)); // 1-9, 0
    if (u >= 0x3A && u <= 0x43) return (uint8_t)(59 + (u - 0x3A)); // F1-F10
    switch (u) {
        case 0x28: return 28;  // Return
        case 0x29: return 1;   // Esc
        case 0x2A: return 14;  // Backspace
        case 0x2B: return 15;  // Tab
        case 0x2C: return 57;  // Space
        case 0x2D: return 12;  // - _
        case 0x2E: return 13;  // = +
        case 0x2F: return 26;  // [ {
        case 0x30: return 27;  // ] }
        case 0x31: return 43;  // \ |
        case 0x32: return 43;  // the ISO # ~ key: the XT's \ |
        case 0x33: return 39;  // ; :
        case 0x34: return 40;  // ' "
        case 0x35: return 41;  // ` ~
        case 0x36: return 51;  // , <
        case 0x37: return 52;  // . >
        case 0x38: return 53;  // / ?
        case 0x39: return 58;  // Caps Lock
        case 0x46: return 55;  // Print Screen -> PrtSc *
        case 0x47: return 70;  // Scroll Lock
        case 0x49: return 82;  // Insert -> keypad 0 Ins
        case 0x4A: return 71;  // Home -> keypad 7 Home
        case 0x4B: return 73;  // Page Up -> keypad 9 PgUp
        case 0x4C: return 83;  // Delete -> keypad . Del
        case 0x4D: return 79;  // End -> keypad 1 End
        case 0x4E: return 81;  // Page Down -> keypad 3 PgDn
        case 0x4F: return 77;  // Right -> keypad 6
        case 0x50: return 75;  // Left -> keypad 4
        case 0x51: return 80;  // Down -> keypad 2
        case 0x52: return 72;  // Up -> keypad 8
        case 0x53: return 69;  // Num Lock
        case 0x54: return 53;  // keypad / -> /
        case 0x55: return 55;  // keypad *
        case 0x56: return 74;  // keypad -
        case 0x57: return 78;  // keypad +
        case 0x58: return 28;  // keypad Enter -> Return
        case 0x59: return 79;  // keypad 1
        case 0x5A: return 80;  // keypad 2
        case 0x5B: return 81;  // keypad 3
        case 0x5C: return 75;  // keypad 4
        case 0x5D: return 76;  // keypad 5
        case 0x5E: return 77;  // keypad 6
        case 0x5F: return 71;  // keypad 7
        case 0x60: return 72;  // keypad 8
        case 0x61: return 73;  // keypad 9
        case 0x62: return 82;  // keypad 0
        case 0x63: return 83;  // keypad .
        case 0x64: return 43;  // the ISO key beside left Shift: the XT's \ |
        case 0xE0: return 29;  // left Ctrl
        case 0xE1: return 42;  // left Shift
        case 0xE2: return 56;  // left Alt (Option on a Mac)
        case 0xE4: return 29;  // right Ctrl
        case 0xE5: return 54;  // right Shift
        case 0xE6: return 56;  // right Alt
        default: return 0;
    }
}

void PcVideoBoard::hostKey(uint16_t usage, bool down) {
    const uint8_t code = scanCodeFor(usage);
    if (!code || !enabled_) return;
    kq_.push_back(down ? code : (uint8_t)(code | 0x80));
    deliverKey();
    armKey();
}

void PcVideoBoard::typeText(const uint8_t* p, size_t n) {
    if (!enabled_) return;
    for (size_t i = 0; i < n; ++i) typeAscii(p[i]);
    deliverKey();
    armKey();
}

void PcVideoBoard::typeAscii(uint8_t ch) {
    if (ch >= 128) return;
    const Key k = keyTable().k[ch];
    if (!k.code) return;
    if (k.mods & kModCtrl) kq_.push_back(kCtrlKey);
    if (k.mods & kModShift) kq_.push_back(kShiftKey);
    kq_.push_back(k.code);
    kq_.push_back((uint8_t)(k.code | 0x80));
    if (k.mods & kModShift) kq_.push_back(kShiftKey | 0x80);
    if (k.mods & kModCtrl) kq_.push_back(kCtrlKey | 0x80);
}

// ---------------------------------------------------------------------------
// THE LIGHT PEN (pp. 68-69), from the mouse. Once each field, when the beam reaches the pen's
// dot and that dot is lit, the pen strobes: the 6845 latches its address + 2, and the ready
// flag sets with the data register still 0.
// ---------------------------------------------------------------------------
void PcVideoBoard::pointer(const Display::Pointer& p) {
    if (!lightpen_) return;
    if (p.kind == Display::Pointer::Kind::Up) {
        penOn_ = false;
        if (clock_) clock_->cancel(penEvt_);
        penEvt_ = Clock::kNone;
        return;
    }
    penOn_ = true;
    penX_  = p.x;
    penY_  = (p.y - barHeight()) / 2;        // below the title bar; each scan line twice
    if (clock_ && penEvt_ != Clock::kNone) {
        clock_->cancel(penEvt_);
        penEvt_ = Clock::kNone;
    }
    armPen();
}

void PcVideoBoard::armPen() {
    if (!clock_ || !lightpen_ || !penOn_ || penEvt_ != Clock::kNone) return;
    const Geometry g = geometry();
    const int x = penX_ - g.borderX, y = penY_ - g.borderY;
    if (g.cols == 0 || x < 0 || y < 0 || x >= g.cols * g.dotsPerChar || y >= g.lines) return;
    catchUp();
    uint64_t cclk = crtc_.clocksUntil((uint32_t)y, (uint8_t)(x / g.dotsPerChar));
    if (cclk == 0) cclk = crtc_.fieldLength();
    const uint64_t per = (uint64_t)mclkPerChar();
    const uint64_t mclk = cclk * per > mclkPhase_ ? cclk * per - mclkPhase_ : 1;
    const uint64_t hz = (uint64_t)clock_->hz();
    const uint64_t t = std::max<uint64_t>(1, (mclk * hz + (uint64_t)kMasterHz - 1) / (uint64_t)kMasterHz);
    penEvt_ = clock_->after(t, [this] {
        penEvt_ = Clock::kNone;
        penStrobe();
        armPen();
    });
}

void PcVideoBoard::penStrobe() {
    catchUp();
    if (!penOn_ || dotAt(penX_, penY_) == 0) return;   // a dark dot gives the pen no light
    crtc_.lightPenStrobe();
    ready_ = true;
    intChanged();
}

// ---------------------------------------------------------------------------
// THE PICTURE (Tables 4, 6-8, 10, 31-33; pp. 63-66).
// ---------------------------------------------------------------------------
PcVideoBoard::Geometry PcVideoBoard::geometry() const {
    Geometry g;
    g.dotsPerChar = mclkPerChar();
    g.cols  = std::min<int>(crtc_.reg(1), 2048 / g.dotsPerChar);
    g.lines = std::min<int>(crtc_.reg(6) * (crtc_.reg(9) + 1), 1024);
    g.borderX = kBorderX;
    g.borderY = kBorderY;
    const int w = g.cols ? g.cols * g.dotsPerChar : 640;
    const int h = g.lines ? g.lines : 200;
    g.width  = w + 2 * kBorderX;
    g.height = h + 2 * kBorderY;
    return g;
}

uint8_t PcVideoBoard::textDot(int col, int line, int dot) const {
    const int perRow = crtc_.reg(9) + 1;
    const uint8_t ra = (uint8_t)(line % perRow);
    const uint16_t ma = (uint16_t)((crtc_.rowStart(line / perRow) + col) & 0x3FFF);
    const uint32_t w = ma & 0x1FFF;          // CA0-CA12 (sheet 8)
    const uint8_t ch = ram_[2 * w];
    const uint8_t at = ram_[2 * w + 1];
    const uint8_t glyph = prom_[0x1000 | ((uint32_t)ch << 4) | (ra & 0x0F)];   // MXSEL = 1
    const int bit = mclkPerChar() == 8 ? dot : dot / 2;
    bool on = (glyph >> (7 - bit)) & 1;
    const uint8_t fg = at & 0x0F;
    uint8_t bg = (at >> 4) & 0x07;
    if (mode_ & kBlink) {
        if ((at & 0x80) && ((crtc_.fields() / 8) & 1)) on = false;   // BLINKER, /16 (p. 63)
    } else if (at & 0x80) {
        bg |= 0x08;                          // AT7 as the background intensity
    }
    const bool curBlinkOn = !cursorBlink_ || ((crtc_.fields() / 16) & 1) == 0;   // /32, J5
    if (curBlinkOn && crtc_.cursorAt(ma, ra)) on = true;
    return on ? fg : bg;
}

uint8_t PcVideoBoard::graphicsDot(int col, int line, int dot) const {
    const int perRow = crtc_.reg(9) + 1;
    const uint8_t ra = (uint8_t)(line % perRow);
    const uint16_t ma = (uint16_t)((crtc_.rowStart(line / perRow) + col) & 0x3FFF);
    const uint32_t w = ((uint32_t)(ra & 1) << 12) | (ma & 0x0FFF);   // MA12 = RA0 (U26)
    const uint8_t e = ram_[2 * w];
    const uint8_t o = ram_[2 * w + 1];
    if (mode_ & kSbw2) {                     // 640 x 200: one bit a dot, even byte first
        const int bit = dot < 8 ? (e >> (7 - dot)) & 1 : (o >> (15 - dot)) & 1;
        return bit ? (color_ & 0x0F) : 0;
    }
    const int pix = dot / 2;                 // 320 x 200: two bits a pixel, two dots wide
    const uint8_t byte = pix < 4 ? e : o;
    const int p = (byte >> (6 - 2 * (pix & 3))) & 3;
    if (p == 0) return color_ & 0x0F;        // the background color (Table 32)
    const int g = p & 1, r = (p >> 1) & 1;   // b0 green, b1 red (p. 13)
    const int b = (mode_ & kSbw1) ? g : (color_ >> 5) & 1;   // SELBLU (p. 66)
    const int i = (color_ >> 4) & 1;         // BAKHI
    return (uint8_t)(i << 3 | r << 2 | g << 1 | b);
}

uint8_t PcVideoBoard::dotAt(int x, int y) const {
    if (!(mode_ & kVidEn)) return 0;         // VIDENB off: no video
    const Geometry g = geometry();
    const int dx = x - g.borderX, dy = y - g.borderY;
    if (g.cols == 0 || g.lines == 0 || dx < 0 || dy < 0 || dx >= g.cols * g.dotsPerChar ||
        dy >= g.lines)
        return ((mode_ & kGrph) && (mode_ & kSbw2)) ? 0 : (color_ & 0x0F);   // the border
    const int col = dx / g.dotsPerChar, dot = dx % g.dotsPerChar;
    return (mode_ & kGrph) ? graphicsDot(col, dy, dot) : textDot(col, dy, dot);
}

void PcVideoBoard::paint(Surface& s) const {
    const Geometry g = geometry();
    const int bar = std::min(barHeight(), s.height());
    if (bar) {
        for (int y = 0; y < bar; ++y)
            for (int x = 0; x < s.width(); ++x) s.put(x, y, kBarColor);
        std::string text = barTitle();
        const int cols = s.width() / 8;
        if ((int)text.size() > cols) text.resize((size_t)cols);
        const int left = (s.width() - 8 * (int)text.size()) / 2;
        for (size_t i = 0; i < text.size(); ++i) {
            uint8_t c = (uint8_t)text[i];
            if (c < 0x20 || c > 0x7E) c = '?';
            for (int y = 0; y < bar; ++y) {
                const uint8_t row = prom_[0x1000 | ((uint32_t)c << 4) | (uint32_t)(y / 2)];
                for (int d = 0; d < 8; ++d)
                    if ((row >> (7 - d)) & 1) s.put(left + 8 * (int)i + d, y, 0);
            }
        }
    }
    const int h = s.height() - bar;
    for (int y = 0; y < h; ++y) {
        const int py = (int)((int64_t)y * g.height / h);
        for (int x = 0; x < s.width(); ++x) s.put(x, bar + y, dotAt(x, py));
    }
}

void PcVideoBoard::render() {
    const Geometry g = geometry();
    Surface* s = g_display->acquire(this, id, g.width, windowHeight(), PixelFormat::Indexed8, width_);
    if (!s) return;
    paint(*s);
    g_display->setPalette(this, kPalette);
    g_display->present(this, s);
}

void PcVideoBoard::pump() {
    if (!enabled_) return;
    uint8_t buf[64];
    while (kbd_->readable()) {
        const size_t n = kbd_->read(buf, sizeof buf);
        if (n == 0) break;
        for (size_t i = 0; i < n; ++i) {
            if (keysAscii_)
                typeAscii(buf[i]);
            else
                kq_.push_back(buf[i]);
        }
    }
    catchUp();
    deliverKey();
    armKey();
    armPen();
    if (!g_display) return;
    const uint64_t phase = crtc_.fields() / 8;   // the blink phases change on this count
    if (phase != drawnPhase_) dirty_ = true;
    if (!dirty_ || !g_display->wantsFrame()) return;
    render();
    dirty_ = false;
    drawnPhase_ = phase;
}

// ---------------------------------------------------------------------------
// RESET, POWER, the clock.
// ---------------------------------------------------------------------------
void PcVideoBoard::reset(Reset) {
    catchUp();
    psel_   = true;                          // U2's preset (sheet 5)
    flip_   = false;                         // U13
    mode_   = 0;                             // U47, U48 cleared by -RESET (sheet 6)
    color_  = 0;
    ready_  = false;
    kbData_ = 0;
    kbHeld_ = false;                         // U18 cleared by -RESET (sheet 5)
    crtc_.reset();
    if (clock_) {
        clock_->cancel(penEvt_);
        penEvt_ = Clock::kNone;
    }
    decodeChanged();
    intChanged();
    dirty_ = true;
}

void PcVideoBoard::power() {
    std::fill(ram_.begin(), ram_.end(), 0);
    kq_.clear();
    reset(Reset::PowerOn);
}

void PcVideoBoard::clockAttached() {
    anchorT_   = clock_ ? clock_->now() : 0;
    rem_       = 0;
    mclkPhase_ = 0;
    keyEvt_    = Clock::kNone;
    penEvt_    = Clock::kNone;
}

void PcVideoBoard::configChanged() {
    decodeChanged();
    intChanged();
    dirty_ = true;
}

bool PcVideoBoard::loadFont(const std::string& spec, std::string& err) {
    Image img;
    if (spec.rfind("builtin:", 0) == 0) {
        const BuiltinRom* rom = findRom(spec.substr(8));
        if (!rom) {
            err = "no built-in ROM named '" + spec.substr(8) + "'. SHOW ROMS lists them.";
            return false;
        }
        if (!decodeRom(*rom, 0, img, err)) return false;
    } else {
        std::ifstream f(resolvePath(spec), std::ios::binary);
        if (!f) {
            err = "cannot open '" + spec + "'" + pathNote(spec);
            return false;
        }
        std::vector<uint8_t> raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (looksLikeHex(raw)) {
            if (!loadHex(raw, img, err)) { err = spec + ": " + err; return false; }
        } else if (looksLikeSrec(raw)) {
            if (!loadSrec(raw, img, err)) { err = spec + ": " + err; return false; }
        } else {
            loadBin(raw, 0, img);
        }
    }
    if (img.empty()) {
        err = spec + ": no bytes";
        return false;
    }
    if (img.bytes.rbegin()->first >= 0x2000) {
        err = spec + ": runs past 1FFF. The character generator (U6) is a 2764, 8K.";
        return false;
    }
    std::fill(prom_.begin(), prom_.end(), 0xFF);
    for (const auto& [a, b] : img.bytes) prom_[a] = b;
    dirty_ = true;
    return true;
}

// ---------------------------------------------------------------------------
// Properties: the switches and jumpers (Tables 1, 3, 11, 13, 20; pp. 3, 18, 63).
// ---------------------------------------------------------------------------
std::vector<Property> PcVideoBoard::properties() {
    std::vector<Property> p;
    {
        Property x;
        x.name  = "memory";
        x.help  = "S1-2..9: the 16K block of screen RAM, A21-A14 (default 0B8000, IBM's color "
                  "address). A multiple of 4000 below 400000";
        x.kind  = Kind::Int;
        x.radix = 16;
        x.min   = 0;
        x.max   = 0x3FC000;
        x.get   = [this] { return Value::ofInt(memory_); };
        x.set   = [this](const Value& v, std::string& err) {
            if (v.i() % 0x4000) {
                err = "the memory block starts on a 16K boundary (a multiple of 4000)";
                return false;
            }
            memory_ = (uint32_t)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name  = "ports";
        x.help  = "S2-1..8: the 16 ports, A11-A4 (default 03D0, IBM's color ports). A multiple "
                  "of 10 below 1000";
        x.kind  = Kind::Int;
        x.radix = 16;
        x.min   = 0;
        x.max   = 0x0FF0;
        x.get   = [this] { return Value::ofInt(ports_); };
        x.set   = [this](const Value& v, std::string& err) {
            if (v.i() & 0x0F) {
                err = "the ports start on a 16-port boundary (a multiple of 10)";
                return false;
            }
            ports_ = (uint16_t)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "wom";
        x.help = "S1-1 OFF: write-only memory. The board takes writes; another RAM board at the "
                 "same address answers reads (off: read and write)";
        x.kind = Kind::Bool;
        x.get  = [this] { return Value::ofBool(wom_); };
        x.set  = [this](const Value& v, std::string&) {
            wom_ = v.b();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "phantom";
        x.help = "S2-10 ON: pull PHANTOM* while the screen RAM is addressed (off, as normally set)";
        x.kind = Kind::Bool;
        x.get  = [this] { return Value::ofBool(phantom_); };
        x.set  = [this](const Value& v, std::string&) {
            phantom_ = v.b();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name    = "monitor";
        x.help    = "S2-9: color (OFF) or mono (ON). Only color is built so far";
        x.kind    = Kind::Enum;
        x.choices = {"color", "mono"};
        x.get     = [] { return Value::ofStr("color"); };
        x.set     = [](const Value& v, std::string& err) {
            if (v.s() == "mono") {
                err = "monochrome mode (S2-9 ON) is not built yet: only color";
                return false;
            }
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name  = "select";
        x.help  = "J3: the bit of port 0 that enables this board, 0-7 (default 0)";
        x.kind  = Kind::Int;
        x.radix = 10;
        x.min   = 0;
        x.max   = 7;
        x.get   = [this] { return Value::ofInt(select_); };
        x.set   = [this](const Value& v, std::string&) {
            select_ = (int)v.i();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "flip";
        x.help = "J2 A-C: port 1 can move the board to the monochrome addresses (on). Off is J2 B-C";
        x.kind = Kind::Bool;
        x.get  = [this] { return Value::ofBool(flipOk_); };
        x.set  = [this](const Value& v, std::string&) {
            flipOk_ = v.b();
            if (!flipOk_) flip_ = false;
            return true;
        };
        p.push_back(std::move(x));
    }
    p.push_back(irqJumperProperty(
        "interrupt", "J4: the VI line the keyboard and light pen pull (vi5, CompuPro's standard)",
        irq_));
    {
        Property x;
        x.name = "cursor_blink";
        x.help = "J5 installed: the cursor blinks at the board's rate (on). Off: a steady cursor";
        x.kind = Kind::Bool;
        x.get  = [this] { return Value::ofBool(cursorBlink_); };
        x.set  = [this](const Value& v, std::string&) {
            cursorBlink_ = v.b();
            dirty_ = true;
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "font";
        x.help = "The character generator, U6 (a 2764): builtin:pcvideo (default), or a file "
                 "(binary, Intel HEX or S-records) in the same layout";
        x.kind = Kind::Str;
        x.get  = [this] { return Value::ofStr(font_); };
        x.set  = [this](const Value& v, std::string& err) {
            if (!loadFont(v.s(), err)) return false;
            font_ = v.s();
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name    = "lightpen";
        x.help    = "The light pen: off (default), or mouse (the left button over the window is "
                    "the pen and its switch)";
        x.kind    = Kind::Enum;
        x.choices = {"off", "mouse"};
        x.get     = [this] { return Value::ofStr(lightpen_ ? "mouse" : "off"); };
        x.set     = [this](const Value& v, std::string&) {
            lightpen_ = v.s() == "mouse";
            if (!lightpen_) penOn_ = false;
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "title";
        x.help = "The text on the window's title bar. Empty = \"" + productName() + titleIdSuffix() + "\"";
        x.kind = Kind::Str;
        x.get  = [this] { return Value::ofStr(title_); };
        x.set  = [this](const Value& v, std::string&) {
            title_ = v.s();
            dirty_ = true;
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name = "titlebar";
        x.help = "Show the title on a bar at the top of the window, in the board's own font";
        x.kind = Kind::Bool;
        x.get  = [this] { return Value::ofBool(titlebar_); };
        x.set  = [this](const Value& v, std::string&) {
            titlebar_ = v.b();
            dirty_ = true;
            return true;
        };
        p.push_back(std::move(x));
    }
    p.push_back(Display::widthProperty(width_));
    return p;
}

std::vector<std::string> PcVideoBoard::statusLines() const {
    const char* what = !(mode_ & kVidEn)     ? "video off"
                       : !(mode_ & kGrph)    ? ((mode_ & kHres) ? "80x25 text" : "40x25 text")
                       : (mode_ & kSbw2)     ? "640x200 graphics"
                                             : "320x200 graphics";
    char buf[200];
    std::snprintf(buf, sizeof buf,
                  "mode %02X (%s)  color %02X  %s  %s  keyboard %s, %zu queued  cursor %04X",
                  mode_, what, color_, psel_ ? "selected" : "not selected",
                  flip_ ? "flipped" : "not flipped", ready_ ? "ready" : "empty", kq_.size(),
                  crtc_.cursorAddress());
    return {buf};
}

std::vector<MapEntry> PcVideoBoard::ioMap() const {
    const uint32_t b = ports_;
    return {
        {b + 0, b + 0, "write", "board select (J3 bit)"},
        {b + 1, b + 1, "write", "address flip, D0"},
        {b + 4, b + 4, "write", "6845 address register"},
        {b + 5, b + 5, "read/write", "6845 data register"},
        {b + 8, b + 8, "write", "mode register"},
        {b + 9, b + 9, "write", "color register"},
        {b + 10, b + 10, "read", "status"},
        {b + 11, b + 11, "write", "clear keyboard; D7 holds the keyboard clock"},
        {b + 12, b + 12, "read/write", "keyboard data / set the ready flag"},
    };
}

std::vector<MapEntry> PcVideoBoard::memMap() const {
    return {{memory_, memory_ + 0x3FFF, "PC Video screen RAM",
             wom_ ? "16K, write only" : "16K: even bytes characters, odd bytes attributes"}};
}

// ---------------------------------------------------------------------------
// The keyboard unit.
// ---------------------------------------------------------------------------
std::vector<UnitDef> PcVideoBoard::units() const {
    return {{"kbd", UnitKind::Serial, kbd_->describe()}};
}

std::vector<Property> PcVideoBoard::boardUnitProperties(const std::string& unit) {
    if (unit != "kbd") return {};
    std::vector<Property> p;
    {
        Property x;
        x.name = "connect";
        x.help = "The endpoint the keyboard's codes come from (CONNECT sets this)";
        x.kind = Kind::Str;
        x.get  = [this] { return Value::ofStr(kbd_->describe()); };
        auto resolve =
            rebasingResolver(g_resolver, [this](const std::string& s) { return resolvePath(s); });
        x.set = [this, resolve](const Value& v, std::string& err) {
            if (!resolve) {
                err = "no endpoint resolver installed";
                return false;
            }
            auto s = resolve(v.s(), err);
            if (!s) return false;
            kbd_ = std::move(s);
            return true;
        };
        p.push_back(std::move(x));
    }
    {
        Property x;
        x.name    = "keys";
        x.help    = "What the endpoint's bytes are: ascii (characters, typed as a PC keyboard "
                    "would; default) or scancodes (as the keyboard sends them)";
        x.kind    = Kind::Enum;
        x.choices = {"ascii", "scancodes"};
        x.get     = [this] { return Value::ofStr(keysAscii_ ? "ascii" : "scancodes"); };
        x.set     = [this](const Value& v, std::string&) {
            keysAscii_ = v.s() == "ascii";
            return true;
        };
        p.push_back(std::move(x));
    }
    return p;
}

bool PcVideoBoard::connect(const std::string& unit, const std::string& ep, std::string& err) {
    if (unit != "kbd") {
        err = "the PC Video has one unit, kbd";
        return false;
    }
    if (!g_resolver) {
        err = "no endpoint resolver installed";
        return false;
    }
    auto s = rebasingResolver(g_resolver, [this](const std::string& p) { return resolvePath(p); })(ep, err);
    if (!s) return false;
    kbd_ = std::move(s);
    return true;
}

bool PcVideoBoard::connectStream(const std::string& unit, std::unique_ptr<ByteStream> s,
                                 std::string& err) {
    if (unit != "kbd") {
        err = "the PC Video has one unit, kbd";
        return false;
    }
    kbd_ = std::move(s);
    return true;
}

bool PcVideoBoard::disconnect(const std::string& unit, std::string& err) {
    if (unit != "kbd") {
        err = "the PC Video has one unit, kbd";
        return false;
    }
    kbd_ = std::make_unique<NullStream>();
    return true;
}

ByteStream* PcVideoBoard::unitStream(const std::string& unit) {
    return unit == "kbd" ? kbd_.get() : nullptr;
}

// ---------------------------------------------------------------------------
// SNAPSHOT/RESTORE (DESIGN.md 13). The straps, the endpoint and the light pen (host input) do
// not travel.
// ---------------------------------------------------------------------------
void PcVideoBoard::serialize(StateWriter& w) const {
    Board::serialize(w);
    w.boolean(psel_);
    w.boolean(flip_);
    w.u8(mode_);
    w.u8(color_);
    w.raw(ram_.data(), ram_.size());
    crtc_.serialize(w);
    w.boolean(ready_);
    w.u8(kbData_);
    w.boolean(kbHeld_);
    w.u32((uint32_t)kq_.size());
    for (uint8_t b : kq_) w.u8(b);
    w.u64(nextKeyAt_);
    w.u64(anchorT_);
    w.u64(rem_);
    w.u64(mclkPhase_);
}

void PcVideoBoard::deserialize(StateReader& r) {
    Board::deserialize(r);
    psel_  = r.boolean();
    flip_  = r.boolean();
    mode_  = r.u8() & 0x3F;
    color_ = r.u8() & 0x3F;
    r.raw(ram_.data(), ram_.size());
    crtc_.deserialize(r);
    ready_  = r.boolean();
    kbData_ = r.u8();
    kbHeld_ = r.boolean();
    kq_.clear();
    const uint32_t n = r.u32();
    for (uint32_t i = 0; i < n; ++i) kq_.push_back(r.u8());
    nextKeyAt_ = r.u64();
    anchorT_   = r.u64();
    rem_       = r.u64();
    mclkPhase_ = r.u64();
    keyEvt_ = penEvt_ = Clock::kNone;        // the clock's queue does not travel
    decodeChanged();
    intChanged();
    dirty_ = true;
    armKey();
}

} // namespace altair
