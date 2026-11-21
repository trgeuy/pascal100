#include "test.h"

#include "boards/compupro-pcvideo.h"
#include "core/machine.h"
#include "core/statefile.h"
#include "host/display_null.h"
#include "host/stream.h"

#include <string>
#include <vector>

using namespace altair;

namespace {

constexpr uint32_t SCREEN = 0x0B8000;
constexpr uint16_t PORTS  = 0x03D0;

// A keyboard endpoint that hands over a fixed string.
struct Feed : ByteStream {
    std::string data;
    size_t      at = 0;
    explicit Feed(std::string d) : data(std::move(d)) {}
    std::string describe() const override { return "feed"; }
    size_t read(uint8_t* buf, size_t n) override {
        size_t k = 0;
        while (k < n && at < data.size()) buf[k++] = (uint8_t)data[at++];
        return k;
    }
    size_t write(const uint8_t*, size_t n) override { return n; }
    bool readable() const override { return at < data.size(); }
    bool writable() const override { return true; }
};

struct Rig {
    Machine       m;
    PcVideoBoard* pv = nullptr;
    explicit Rig(std::initializer_list<std::pair<const char*, const char*>> props = {}) {
        std::string err;
        pv = dynamic_cast<PcVideoBoard*>(m.add("pcvideo", "pcv0", err));
        CHECK(pv != nullptr, ("the machine takes a pcvideo: " + err).c_str());
        for (auto& [k, v] : props)
            CHECK(setProperty(*pv, k, v, err), (std::string(k) + ": " + err).c_str());
        m.bus.setVerify(true);
        m.power();
    }
    void    out(int reg, uint8_t v) { m.bus.ioWrite((uint16_t)(PORTS + reg), v); }
    uint8_t in(int reg) { return m.bus.ioRead((uint16_t)(PORTS + reg)); }
    void    crtc(int r, uint8_t v) {
        out(4, (uint8_t)r);
        out(5, v);
    }
    // The 80x25 text timing: 114 characters a line, 32 rows of 8 scan lines and 6 adjust
    // lines = 262 lines a field; 80 x 25 displayed; VSYNC at row 28.
    void text80() {
        const uint8_t v[12] = {0x71, 0x50, 0x5A, 0x0A, 0x1F, 0x06, 0x19, 0x1C, 0x02, 0x07, 0x06, 0x07};
        for (int r = 0; r < 12; ++r) crtc(r, v[r]);
        crtc(12, 0);
        crtc(13, 0);
        out(8, 0x09);                         // HRES + VIDENB
    }
    void run(uint64_t t) { m.clock.advance(t); }
    uint64_t hz() const { return (uint64_t)m.clock.hz(); }
};

} // namespace

void test_pcvideo() {
    SECTION("pcvideo: the screen RAM at 0B8000-0BBFFF, the ports at 03D0, all 16 bits decoded");
    {
        Rig g;
        g.m.bus.memWriteX(SCREEN + 0x0000, 0x41);
        g.m.bus.memWriteX(SCREEN + 0x3FFF, 0x1E);
        CHECK(g.m.bus.memReadX(SCREEN) == 0x41 && g.m.bus.memReadX(SCREEN + 0x3FFF) == 0x1E,
              "the first and last byte of the 16K block");
        CHECK(g.pv->ram(0) == 0x41, "...in the board's RAM");
        g.m.bus.setUnclaimedPolicy(Unclaimed::Silent);
        CHECK(g.m.bus.memReadX(SCREEN + 0x4000) == 0xFF, "0BC000 is not the board's");
        g.crtc(14, 0x12);
        g.out(4, 14);
        CHECK(g.in(5) == 0x12, "6845 R14 through 03D4/03D5");
        CHECK(g.m.bus.ioRead(0x00D5) == 0xFF, "00D5 is not 03D5: A8-A15 are decoded");
        g.out(4, 0);
        CHECK(g.in(5) == 0xFF, "a write-only 6845 register reads FF (the bus floats)");
        CHECK((g.in(2) & 0xFF) == 0xFF, "an unused port reads FF");
    }

    SECTION("pcvideo: port 0 disables the board; only port 0 then works (Table 13, sheet 5)");
    {
        Rig g({{"select", "2"}});
        g.m.bus.setUnclaimedPolicy(Unclaimed::Silent);
        g.m.bus.memWriteX(SCREEN, 0x55);
        g.out(0, 0x00);
        CHECK(!g.pv->selected(), "bit 2 = 0: not selected");
        CHECK(g.m.bus.memReadX(SCREEN) == 0xFF, "the RAM is gone from the bus");
        g.out(4, 14);
        CHECK(g.in(5) == 0xFF, "so is the 6845");
        g.out(0, 0x04);
        CHECK(g.pv->selected() && g.m.bus.memReadX(SCREEN) == 0x55, "bit 2 = 1: back, RAM kept");
    }

    SECTION("pcvideo: port 1 moves the board to 0B0000 and 03B0; J2 B-C stops it (p. 18)");
    {
        Rig g;
        g.m.bus.setUnclaimedPolicy(Unclaimed::Silent);
        g.out(1, 0x01);
        g.m.bus.memWriteX(0x0B0000, 0x77);
        CHECK(g.pv->flipped() && g.pv->ram(0) == 0x77, "the RAM answers at 0B0000");
        g.m.bus.ioWrite(0x03B4, 14);
        g.m.bus.ioWrite(0x03B5, 0x2A);
        CHECK(g.m.bus.ioRead(0x03B5) == 0x2A, "the 6845 at 03B4/03B5");
        CHECK(g.m.bus.memReadX(SCREEN) == 0xFF, "and not at 0B8000");
        g.m.bus.ioWrite(0x03B1, 0x00);
        CHECK(!g.pv->flipped() && g.m.bus.memReadX(SCREEN) == 0x77, "0 to port 1: back again");

        Rig j({{"flip", "false"}});
        j.out(1, 0x01);
        CHECK(!j.pv->flipped(), "flip = false (J2 B-C): port 1 does nothing");
    }

    SECTION("pcvideo: write-only memory -- writes land, reads are another board's (S1-1 OFF)");
    {
        Rig g({{"wom", "true"}});
        g.m.bus.setUnclaimedPolicy(Unclaimed::Silent);
        g.m.bus.memWriteX(SCREEN + 2, 0x33);
        CHECK(g.pv->ram(2) == 0x33, "the write reached the board");
        CHECK(g.m.bus.memReadX(SCREEN + 2) == 0xFF, "the read did not: nothing else is there");
        CHECK(!g.m.bus.memRead16(SCREEN).sixtn, "no SIXTN* in write-only mode");
    }

    SECTION("pcvideo: 16-bit transfers -- SIXTN*, the odd byte from DI0-DI7 (p. 52, sheet 7)");
    {
        Rig g;
        CHECK(g.m.bus.memWrite16(SCREEN + 4, 0x48, 0x1F), "a 16-bit write is answered");
        CHECK(g.pv->ram(4) == 0x48 && g.pv->ram(5) == 0x1F, "character, then attribute");
        Transfer16 t = g.m.bus.memRead16(SCREEN + 4);
        CHECK(t.sixtn && t.even == 0x48 && t.odd == 0x1F, "and read back as one word");
    }

    SECTION("pcvideo: status -- D3 vertical sync, D0 blanking, D4-D7 float (Table 19, sheet 6)");
    {
        Rig g;
        g.text80();
        g.run(g.hz() / 10);                    // past the first field after RESET
        // Sample the status every 10 us for 50 ms: about three fields.
        int vsOn = 0, blank = 0, n = 0, edges = 0;
        bool last = false;
        for (uint64_t t = 0; t < g.hz() / 20; t += g.hz() / 100000, ++n) {
            const uint8_t s = g.in(10);
            CHECK((s & 0xF0) == 0xF0, "D4-D7 read 1");
            if (s & 0x08) ++vsOn;
            if (s & 0x01) ++blank;
            if ((s & 0x08) && !last) ++edges;
            last = s & 0x08;
            g.run(g.hz() / 100000);
        }
        // A field is 114 x 262 characters of 8 master clocks: 16.69 ms. VSYNC is 16 lines of
        // 114 characters: 1.02 ms, 6.1 percent of the field. Blanking is everything outside
        // 80 x 200 of 114 x 262: 46 percent.
        CHECK(edges == 3, "three VSYNC pulses in 50 ms: a 59.9 Hz field");
        CHECK(vsOn * 1000 / n > 50 && vsOn * 1000 / n < 72, "VSYNC about 6 percent of the time");
        CHECK(blank * 100 / n > 40 && blank * 100 / n < 52, "D0 set about 46 percent of the time");
    }

    SECTION("pcvideo: wait states -- a few T-states, more for the 6845 (pp. 44-48)");
    {
        Rig g;
        g.text80();
        uint32_t memMax = 0, crtcMax = 0, memMin = 99;
        for (int i = 0; i < 64; ++i) {
            g.m.bus.takeWaitStates();
            g.m.bus.memReadX(SCREEN);
            const uint32_t w = g.m.bus.takeWaitStates();
            memMax = std::max(memMax, w);
            memMin = std::min(memMin, w);
            g.out(4, 14);
            g.m.bus.takeWaitStates();
            g.in(5);
            crtcMax = std::max(crtcMax, g.m.bus.takeWaitStates());
            g.run(1);
        }
        // At 2 MHz a host T-state is 7.16 master clocks. Memory: up to 7 + 1 + 2 = 10 master
        // clocks (2 T); the 6845: up to 7 + 1 + 8 = 16 (3 T).
        CHECK(memMin >= 1 && memMax <= 2, "memory: 1-2 T-states at 2 MHz");
        CHECK(crtcMax >= 2 && crtcMax <= 3, "the 6845: up to 3 T-states");
    }

    SECTION("pcvideo: the keyboard -- one code, VI5, held until port 11 (pp. 22, 69)");
    {
        Rig g;
        const uint8_t codes[] = {0x1E, 0x9E};  // A down, A up
        g.pv->keyCodes(codes, 2);
        CHECK(g.pv->ready() && (g.in(10) & 0x02), "the first code: ready (status D1)");
        CHECK((g.m.bus.viLines() & 0x20) != 0, "VI5 pulled");
        CHECK(g.in(12) == 0x1E && g.in(12) == 0x1E, "port 12 reads it, and a read does not clear");
        g.out(11, 0x00);
        CHECK(!g.pv->ready() && (g.m.bus.viLines() & 0x20) == 0, "port 11 clears ready and VI5");
        CHECK(g.in(12) == 0x00, "...and the data register reads 0");
        g.run(g.hz() / 2000);
        CHECK(!g.pv->ready(), "the keyboard waits: not after 0.5 ms");
        g.run(g.hz() / 1000);
        CHECK(g.pv->ready() && g.in(12) == 0x9E, "the next code after 1 ms");
        g.out(11, 0x80);
        const uint8_t more[] = {0x1F};
        g.pv->keyCodes(more, 1);
        g.run(g.hz() / 100);
        CHECK(!g.pv->ready(), "port 11 D7 = 1 holds the keyboard clock: nothing comes");
        g.out(11, 0x00);
        g.run(g.hz() / 100);
        CHECK(g.pv->ready() && g.in(12) == 0x1F, "D7 = 0: it comes");
        g.out(12, 0);
        CHECK(g.pv->ready(), "a write to port 12 sets ready");
    }

    SECTION("pcvideo: keys = ascii -- a character is typed with Shift or Ctrl (PCKEY.DVR)");
    {
        Rig g;
        std::string err;
        CHECK(g.pv->connectStream("kbd", std::make_unique<Feed>("aA\x03"), err), err.c_str());
        g.pv->pump();
        std::vector<uint8_t> got;
        for (int i = 0; i < 20 && (g.pv->ready() || g.pv->keysQueued()); ++i) {
            g.run(g.hz() / 500);
            if (g.pv->ready()) {
                got.push_back(g.in(12));
                g.out(11, 0);
            }
        }
        const std::vector<uint8_t> want = {0x1E, 0x9E,                          // a
                                           0x2A, 0x1E, 0x9E, 0xAA,              // Shift A
                                           0x1D, 0x2E, 0xAE, 0x9D};             // Ctrl C
        CHECK(got == want, "a; Shift-A; Ctrl-C, each key down then up");
    }

    SECTION("pcvideo: text -- the PROM, the attribute, blink, the cursor (Table 4, pp. 63-64)");
    {
        Rig g;
        g.text80();
        g.run(g.hz() / 10);
        g.m.bus.memWrite16(SCREEN + 0, 'H', 0x1E);     // yellow on blue
        g.m.bus.memWrite16(SCREEN + 2, 0xDB, 0x8C);    // a full block, light red, AT7 set
        g.out(9, 0x02);                                // border: green
        const auto geo = g.pv->geometry();
        CHECK(geo.width == 640 + 64 && geo.height == 200 + 32, "80 x 8 dots by 25 x 8 lines, a border");
        const int bx = geo.borderX, by = geo.borderY;
        CHECK(g.pv->dotAt(0, 0) == 0x02, "the border in the color register's low nibble");
        // 'H' in builtin:pcvideo's color half: ##...##. on scan line 2, #######. (the bar) on
        // scan line 3 (bit 7 left).
        bool rowOk = true;
        for (int d = 0; d < 8; ++d) {
            rowOk = rowOk && g.pv->dotAt(bx + d, by + 2) == (((0xC6 >> (7 - d)) & 1) ? 0x0E : 0x01);
            rowOk = rowOk && g.pv->dotAt(bx + d, by + 3) == (((0xFE >> (7 - d)) & 1) ? 0x0E : 0x01);
        }
        CHECK(rowOk, "'H' scan lines 2 and 3: yellow dots on blue");
        CHECK(g.pv->dotAt(bx + 8, by) == 0x0C, "the block is light red");
        // AT7 with ENBBLINK = 0 is the background's intensity: the block shows no background,
        // so look at the attribute in a space.
        g.m.bus.memWrite16(SCREEN + 4, ' ', 0x90);
        CHECK(g.pv->dotAt(bx + 16, by) == 0x09, "AT7 = background intensity: light blue (ENBBLINK 0)");
        g.out(8, 0x29);                                 // ENBBLINK
        CHECK(g.pv->dotAt(bx + 16, by) == 0x01, "ENBBLINK 1: AT7 no longer brightens it");
        bool shown = false, blanked = false;
        for (int f = 0; f < 32; ++f) {
            const uint8_t d = g.pv->dotAt(bx + 8, by);
            shown = shown || d == 0x0C;
            blanked = blanked || d == 0x00;
            g.run(g.hz() / 60);
            g.pv->catchUp();                             // as pump() does before a frame
        }
        CHECK(shown && blanked, "ENBBLINK 1, AT7 set: the block blinks to its background");
        g.out(8, 0x09);
        g.crtc(14, 0);
        g.crtc(15, 4);                                  // the cursor on the space at column 4
        g.crtc(10, 0x06);
        g.crtc(11, 0x07);
        g.m.bus.memWrite16(SCREEN + 8, ' ', 0x07);
        g.crtc(10, 0x06);
        bool cursor = false;
        for (int f = 0; f < 40 && !cursor; ++f) {
            cursor = g.pv->dotAt(bx + 32, by + 6) == 0x07 && g.pv->dotAt(bx + 32, by + 5) == 0x00;
            g.run(g.hz() / 60);
            g.pv->catchUp();
        }
        CHECK(cursor, "the cursor fills scan lines 6-7 of its cell in the foreground");
    }

    SECTION("pcvideo: 320 x 200 -- b0 green, b1 red, the palette bit, odd lines at 2000h (Table 6-8)");
    {
        Rig g;
        g.text80();
        g.crtc(0, 0x38);
        g.crtc(1, 0x28);
        g.crtc(2, 0x2D);
        g.crtc(4, 0x7F);
        g.crtc(6, 0x64);
        g.crtc(7, 0x70);
        g.crtc(9, 0x01);
        g.out(8, 0x0A);                                 // GRPH + VIDENB
        g.run(g.hz() / 10);
        g.m.bus.memWriteX(SCREEN + 0x0000, 0x1B);       // pixels 0, 1, 2, 3
        g.m.bus.memWriteX(SCREEN + 0x2000, 0xC0);       // line 1, pixel 0 = 3
        g.out(9, 0x01);                                 // background blue, palette 0
        const auto geo = g.pv->geometry();
        const int bx = geo.borderX, by = geo.borderY;
        CHECK(geo.width == 640 + 64 && geo.height == 200 + 32, "320 pixels of two dots, 200 lines");
        auto px = [&](int p, int line) { return g.pv->dotAt(bx + 2 * p, by + line); };
        CHECK(px(0, 0) == 0x01 && px(1, 0) == 0x02 && px(2, 0) == 0x04 && px(3, 0) == 0x06,
              "palette 0: background, green, red, brown");
        CHECK(px(0, 1) == 0x06, "line 1 comes from 2000h (MA12 = RA0)");
        g.out(9, 0x31);                                 // palette 1, intensity
        CHECK(px(1, 0) == 0x0B && px(2, 0) == 0x0D && px(3, 0) == 0x0F,
              "palette 1 with intensity: light cyan, light magenta, white");
        g.out(8, 0x1E);                                 // 640 x 200
        g.out(9, 0x0F);
        CHECK(g.pv->dotAt(bx + 3, by) == 0x0F && g.pv->dotAt(bx + 0, by) == 0x00,
              "640 x 200: a set bit in the color register's color, a clear one black");
        CHECK(g.pv->dotAt(0, 0) == 0x00, "640 x 200: the border is black");
    }

    SECTION("pcvideo: the light pen from the mouse -- the address + 2, only on a lit dot");
    {
        Rig g({{"lightpen", "mouse"}});
        g.text80();
        g.run(g.hz() / 10);
        g.m.bus.memWrite16(SCREEN + 10, 0xDB, 0x0F);    // a white block at column 5
        const auto geo = g.pv->geometry();
        Display::Pointer p;
        p.kind = Display::Pointer::Kind::Down;
        p.x = geo.borderX + 5 * 8 + 3;
        p.y = (geo.borderY + 2) * 2;                    // the window doubles each scan line
        g.pv->pointer(p);
        CHECK((g.in(10) & 0x04) == 0, "the switch is closed: status D2 = 0");
        g.run(g.hz() / 30);
        CHECK(g.pv->ready(), "the pen strobed within a field");
        g.out(4, 16);
        const uint8_t h = g.in(5);
        g.out(4, 17);
        const uint8_t l = g.in(5);
        CHECK(((h << 8) | l) == 5 + 2, "R16/R17 = the block's address + 2");
        CHECK(g.in(12) == 0x00, "the data register reads 0: not a key");
        g.out(11, 0);
        p.x = geo.borderX + 6 * 8 + 3;                  // a dark cell
        g.pv->pointer(p);
        g.run(g.hz() / 10);
        CHECK(!g.pv->ready(), "no strobe on a dark dot");
        p.kind = Display::Pointer::Kind::Up;
        g.pv->pointer(p);
        CHECK((g.in(10) & 0x04) != 0, "up: the switch opens");
    }

    SECTION("pcvideo: a frame through the display: the picture doubled in height");
    {
        Rig g;
        g.text80();
        g.run(g.hz() / 10);
        NullDisplay own;
        Display* before = PcVideoBoard::display();
        PcVideoBoard::setDisplay(&own);
        NullDisplay* d = &own;
        g.pv->pump();
        const Surface* s = d->surface(g.pv);
        CHECK(s && s->width() == 704 && s->height() == 464, "704 x 464: 232 lines, each twice");
        const uint64_t f = d ? d->frames(g.pv) : 0;
        g.pv->pump();
        CHECK(d && d->frames(g.pv) == f, "nothing changed: no new frame");
        g.m.bus.memWriteX(SCREEN, 'X');
        g.pv->pump();
        CHECK(d && d->frames(g.pv) == f + 1, "a write to the RAM: a new frame");
        PcVideoBoard::setDisplay(before);
    }

    SECTION("pcvideo: straps -- the font, mono refused, the address checks");
    {
        Rig g;
        std::string err;
        CHECK(!setProperty(*g.pv, "monitor", "mono", err), "monitor = mono is refused (stage 3)");
        CHECK(!setProperty(*g.pv, "font", "builtin:nosuch", err), "an unknown built-in font");
        CHECK(!setProperty(*g.pv, "memory", "0B9000", err), "memory not on a 16K boundary");
        CHECK(!setProperty(*g.pv, "ports", "03D8", err), "ports not on a 16-port boundary");
        CHECK(setProperty(*g.pv, "memory", "0C0000", err), err.c_str());
    }

    SECTION("pcvideo: RESET -- selected, not flipped, mode and color 0, the 6845 kept");
    {
        Rig g;
        g.text80();
        g.out(1, 1);
        g.m.bus.ioWrite(0x03B0, 0x00);                  // port 0 is now at 03B0: deselect
        g.m.reset(Reset::Bus);
        CHECK(g.pv->selected() && !g.pv->flipped(), "U2 preset, U13 cleared");
        CHECK(g.pv->mode() == 0 && g.pv->colorReg() == 0, "U47 and U48 cleared");
        CHECK(g.pv->crtc().reg(0) == 0x71, "the 6845's registers stay");
    }

    SECTION("pcvideo: a snapshot carries the RAM, the registers, the 6845 and the keyboard");
    {
        Rig a;
        a.text80();
        a.out(9, 0x15);
        a.m.bus.memWrite16(SCREEN + 6, 'Q', 0x4F);
        const uint8_t codes[] = {0x10, 0x90, 0x11};
        a.pv->keyCodes(codes, 3);
        a.run(12345);
        StateWriter w;
        a.pv->serialize(w);
        Rig b;
        StateReader r(w.data());
        b.pv->deserialize(r);
        CHECK(b.pv->ram(6) == 'Q' && b.pv->ram(7) == 0x4F, "the RAM");
        CHECK(b.pv->mode() == 0x09 && b.pv->colorReg() == 0x15, "the mode and color registers");
        CHECK(b.pv->crtc().reg(1) == 0x50 && b.pv->crtc().hCount() == a.pv->crtc().hCount(),
              "the 6845's registers and counters");
        CHECK(b.pv->ready() && b.pv->keysQueued() == 2, "the keyboard: a code ready, two to come");
    }
}
