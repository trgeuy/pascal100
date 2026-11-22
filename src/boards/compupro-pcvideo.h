#pragma once
//
// The CompuPro (Viasyn) PC Video, board 229D: 16K of screen RAM, a Motorola 6845, an IBM PC
// keyboard port and a light pen port. It copies the IBM Color Graphics Adapter at the bus
// level -- the same memory and port addresses, the same register bits -- and CompuPro's
// Concurrent DOS 8-16 uses it as console 0 and for PC Mode. DESIGN.md 7.4.1 is the design;
// the *PC Video Technical Manual* (A493) and its logic diagram are the source.
//
// COLOR AND MONOCHROME. `monitor` is S2-9 (MXSEL): color (OFF) or mono (ON). Mono selects the
// 16.257 MHz crystal, the 9x14 half of the character PROM, the 9th dot and the monochrome
// attribute logic (underline, reverse: Table 31), and drives a green IBM 5151-type monitor.
//
// TIME. The board runs on its own master clock: 14.31818 MHz (X2) for color, 16.257 MHz (X1)
// for mono (p. 41). It converts host T-states into master clocks with the host divisor,
// remainder carried, and the master clocks into the 6845's character clocks: 8 a character
// for color 80x25 text, 9 for mono 80x25 text, 16 for 40x25 text and graphics (Table 22). Every timing a guest can see comes from that: the status port's display enable
// and vertical sync, the blink rates, the light pen strobe and the wait states.
//
// THE PICTURE is drawn whole, from the state at the time of the frame (DESIGN.md 7.4.1).
//
// THE KEYBOARD is the unit `kbd`, an endpoint. With `keys = ascii` (the default) each byte is
// a character, and the board types it as an IBM PC keyboard would: Shift or Ctrl, the key
// down, the key up. With `keys = scancodes` each byte is a scan code as the keyboard sends
// it. The scan code numbers are those of CompuPro's own driver (PCKEY.DVR).

#include "chips/mc6845.h"
#include "chips/mc6850.h"   // EndpointResolver
#include "core/board.h"
#include "core/clock.h"
#include "host/display.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace altair {

class PcVideoBoard : public Board {
public:
    PcVideoBoard();
    ~PcVideoBoard() override;

    std::string type() const override { return "pcvideo"; }

    // The host video service and the endpoint resolver, installed once by the program
    // (src/main.cpp, tests/main.cpp).
    static void setDisplay(Display* d);
    static Display* display();
    static void setResolver(EndpointResolver r);

    bool    decodes(const BusCycle& c) const override;
    bool    decodesIoHigh(uint8_t port) const override;
    // Port 0 (board select) is written to every PC Video at once: each takes its own bit.
    bool    sharesPort(const BusCycle& c) const override;
    bool    decodesExtended() const override { return true; }
    bool    decodeIsPageUniform() const override { return true; }
    bool    assertsPhantom(const BusCycle& c) const override;
    uint8_t read(const BusCycle& c) override;
    void    write(const BusCycle& c) override;
    bool    answersSixtn(const BusCycle& c) const override;
    void    read16(const BusCycle& c, uint8_t& even, uint8_t& odd) override;
    void    write16(const BusCycle& c) override;
    bool    peek(uint16_t addr, uint8_t& out) const override { return peekX(addr, out); }
    bool    peekX(uint32_t phys, uint8_t& out) const override;
    uint8_t assertsVi() const override { return ready_ ? viBit(irq_) : 0; }

    void reset(Reset) override;
    void power() override;
    void configChanged() override;
    void clockAttached() override;

    std::vector<Property>    properties() override;
    std::vector<std::string> statusLines() const override;
    std::vector<MapEntry>    ioMap() const override;
    std::vector<MapEntry>    memMap() const override;

    std::vector<UnitDef>  units() const override;
    std::vector<Property> boardUnitProperties(const std::string& unit) override;
    bool connect(const std::string& unit, const std::string& endpoint, std::string& err) override;
    bool connectStream(const std::string& unit, std::unique_ptr<ByteStream> s,
                       std::string& err) override;
    bool disconnect(const std::string& unit, std::string& err) override;
    ByteStream* unitStream(const std::string& unit) override;
    void        pump() override;

    void serialize(StateWriter& w) const override;
    void deserialize(StateReader& r) override;

    // The light pen, from the mouse over this board's window (`lightpen = mouse`): the left
    // button down or dragging puts the pen at that dot of the picture and closes its switch;
    // up opens the switch and takes the pen away.
    void pointer(const Display::Pointer& p);

    // Scan codes from the host keyboard, as the keyboard sends them (bit 7 = released).
    void keyCodes(const uint8_t* codes, size_t n);

    // THE WINDOW'S KEYBOARD. The board claims physical keys for its window (Display::
    // claimRawKeys): a key down or up there arrives as a USB HID usage, and the board sends
    // the IBM PC keyboard's code for it. Text pasted into the window arrives as ASCII and is
    // typed as `keys = ascii` types it. The composition root finds the board by its window
    // (forOwner) and calls these (src/main.cpp).
    static PcVideoBoard* forOwner(const void* owner);
    void hostKey(uint16_t usage, bool down);
    void typeText(const uint8_t* p, size_t n);
    // The PC/XT scan code (1-83) for a USB HID usage, or 0 for a key the 83-key keyboard does
    // not have. The keys it lacks a code for (arrows, Home, Insert...) map to the keypad's.
    static uint8_t scanCodeFor(uint16_t usage);

    // ---- For tests and SHOW ----
    Mc6845&  crtc() { return crtc_; }
    uint8_t  ram(uint32_t a) const { return ram_[a & 0x3FFF]; }
    uint8_t  mode() const { return mode_; }
    uint8_t  colorReg() const { return color_; }
    bool     selected() const { return psel_; }
    bool     flipped() const { return flip_; }
    bool     ready() const { return ready_; }
    size_t   keysQueued() const { return kq_.size(); }
    void     catchUp();
    // The picture's size and where the 6845's display area sits in it.
    struct Geometry {
        int dotsPerChar = 8;  // master clocks per character clock: one dot each
        int cols = 0, lines = 0;  // R1 characters, R6 x (R9 + 1) scan lines
        int width = 0, height = 0;  // the whole picture, the border included
        int borderX = 0, borderY = 0;
    };
    Geometry geometry() const;
    // The color index (RGBI, Table 10) of picture dot (x, y), as the next frame draws it.
    uint8_t dotAt(int x, int y) const;
    // THE WINDOW: the title bar (when `titlebar` is on), then the picture with each scan line
    // drawn twice (color) or 1.5 times (mono). The bar is one character row of the board's own
    // font, doubled, in light gray with the title in black, as a terminal window's bar is. The
    // guest never sees it.
    int  barHeight() const { return titlebar_ ? 16 : 0; }
    // Scan lines are drawn at 2x for color (200 lines) and 1.5x for mono (350 lines), so both
    // pictures come out near 4:3. A model decision (DESIGN.md 7.4.1).
    int  windowHeight() const {
        const int h = geometry().height;
        return barHeight() + (mono_ ? (3 * h + 1) / 2 : 2 * h);
    }
    std::string barTitle() const { return title_.empty() ? productName() + titleIdSuffix() : title_; }
    // Render the window into a Surface of geometry().width x windowHeight() (Indexed8).
    void paint(Surface& s) const;
    static constexpr uint8_t kBarColor = 16;   // palette index of the bar's gray

    static constexpr long long kColorHz  = 14318180;  // X2, 14.31818 MHz (p. 41)
    static constexpr long long kMonoHz   = 16257000;  // X1, 16.257 MHz (p. 41)
    // Palette indexes of the green phosphor (mono). 0 is black in both modes.
    static constexpr uint8_t   kMonoOn   = 17;
    static constexpr uint8_t   kMonoHi   = 18;
    long long masterHz() const { return mono_ ? kMonoHz : kColorHz; }
    static constexpr int       kBorderX  = 32;        // the border drawn around the display area
    static constexpr int       kBorderY  = 16;

private:
    // Mode register bits (port 8, Tables 16 and 21).
    static constexpr uint8_t kHres  = 0x01;
    static constexpr uint8_t kGrph  = 0x02;
    static constexpr uint8_t kSbw1  = 0x04;
    static constexpr uint8_t kVidEn = 0x08;
    static constexpr uint8_t kSbw2  = 0x10;
    static constexpr uint8_t kBlink = 0x20;

    bool     memHit(const BusCycle& c) const;
    int      ioReg(const BusCycle& c) const;   // the relative port 0-15, or -1
    uint32_t offset(const BusCycle& c) const { return c.addr & 0x3FFF; }
    // Table 22: HRES text is 8 (color) or 9 (mono) master clocks; graphics and 40x25 are 16.
    int      mclkPerChar() const {
        return ((mode_ & kHres) && !(mode_ & kGrph)) ? (mono_ ? 9 : 8) : 16;
    }
    void     waitFor(bool crtcPort);            // PRDY: the wait for the transfer window
    uint8_t  status() const;
    bool     loadFont(const std::string& spec, std::string& err);
    void     clearKeyboard(uint8_t data);
    void     deliverKey();
    void     armKey();
    void     armPen();
    void     penStrobe();
    void     typeAscii(uint8_t ch);
    void     render();
    uint8_t  textDot(int col, int line, int dot) const;
    uint8_t  graphicsDot(int col, int line, int dot) const;

    // ---- Straps ----
    uint32_t    memory_ = 0x0B8000;   // S1-2..9: A21-A14
    uint16_t    ports_  = 0x03D0;     // S2-1..8: A11-A4
    bool        wom_    = false;      // S1-1 OFF: write-only memory
    bool        phantom_ = false;     // S2-10 ON: pull PHANTOM*
    int         select_ = 0;          // J3: the bit of port 0 that enables this board
    bool        flipOk_ = true;       // J2 A-C: port 1 works
    bool        cursorBlink_ = true;  // J5 installed: the cursor blinks
    bool        mono_ = false;        // S2-9 ON: MXSEL low, monochrome
    IrqJumper   irq_ = IrqJumper::Vi5;  // J4 (CompuPro software expects VI5)
    std::string font_ = "builtin:pcvideo";
    bool        lightpen_ = false;    // the mouse as a light pen
    bool        keysAscii_ = true;    // `kbd` carries characters, not scan codes
    int         width_ = 0;           // the window width (Display::widthProperty)
    std::string title_;               // the title bar's text; empty = "CompuPro PC Video (id)"
    bool        titlebar_ = true;     // draw the title bar

    // ---- Live state ----
    bool     psel_ = true;            // U2: the board select (reset sets it)
    bool     flip_ = false;           // U13: the address flip (reset clears it)
    uint8_t  mode_ = 0;               // U47
    uint8_t  color_ = 0;              // U48
    Mc6845   crtc_;
    std::vector<uint8_t> ram_ = std::vector<uint8_t>(0x4000, 0);
    std::vector<uint8_t> prom_ = std::vector<uint8_t>(0x2000, 0);
    bool     ready_ = false;          // KBDSTB: keyboard or light pen data ready
    uint8_t  kbData_ = 0;             // U19
    bool     kbHeld_ = false;         // U18: port 11 D7 holds the keyboard clock low
    std::deque<uint8_t> kq_;          // codes the keyboard has not sent yet
    uint64_t nextKeyAt_ = 0;          // host T-state: the keyboard may send its next code
    Clock::Handle keyEvt_ = Clock::kNone;
    Clock::Handle penEvt_ = Clock::kNone;

    // The master clock against host time.
    uint64_t anchorT_ = 0;            // host T-state caught up to
    uint64_t rem_ = 0;                // the remainder of the conversion, carried
    uint64_t mclkPhase_ = 0;          // master clocks into the present character clock

    // The light pen (host side; does not travel in a snapshot).
    bool penOn_ = false;              // a pen over the picture
    int  penX_ = 0, penY_ = 0;        // picture dots

    std::unique_ptr<ByteStream> kbd_;
    bool     dirty_ = true;
    uint64_t drawnPhase_ = ~0ull;     // the blink phases the last frame showed
};

} // namespace altair
