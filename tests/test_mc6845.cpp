#include "test.h"

#include "chips/mc6845.h"
#include "core/statefile.h"

#include <vector>

using namespace altair;

namespace {

void set(Mc6845& c, int r, uint8_t v) {
    c.selectRegister((uint8_t)r);
    c.writeData(v);
}

// A small screen that every count in it can be checked by hand:
//   10 characters a line (R0 = 9), 6 displayed (R1), HS at 7 for 2 (R2, R3);
//   10 rows (R4 = 9) of 3 scan lines (R9 = 2), 8 displayed (R6), VS at row 8 (R7),
//   3 adjust lines (R5); start address 0100.
// A field is 10 x 3 + 3 = 33 scan lines, 330 character clocks.
void program(Mc6845& c) {
    const uint8_t v[16] = {9, 6, 7, 2, 9, 3, 8, 8, 0, 2, 0x20, 0, 0x01, 0x00, 0, 0};
    for (int r = 0; r < 16; ++r) set(c, r, v[r]);
    c.reset();
}

// Run past the first field after RESET, which shows no DE or CURSOR (p. 9).
void settle(Mc6845& c) { c.advance(c.fieldLength()); }

} // namespace

void test_mc6845() {
    SECTION("mc6845: the register file -- widths, the address register, what reads back (Table 2)");
    {
        Mc6845 c;
        c.selectRegister(0x2E);
        CHECK(c.addressRegister() == 0x0E, "the address register holds 5 bits");
        c.writeData(0xFF);
        uint8_t v = 0;
        CHECK(c.readData(v) && v == 0x3F, "R14 (cursor H) reads back its 6 bits");
        set(c, 15, 0xAB);
        CHECK(c.readData(v) && v == 0xAB, "R15 (cursor L) reads back");
        set(c, 3, 0xF5);
        CHECK(c.reg(3) == 0x05, "R3 holds 4 bits");
        set(c, 10, 0xFF);
        CHECK(c.reg(10) == 0x7F, "R10 holds 7 bits");
        v = 0x5A;
        CHECK(!c.readData(v) && v == 0x5A, "R10 is write-only: the chip does not drive the bus");
        set(c, 16, 0x12);
        CHECK(c.readData(v) && v == 0x00, "R16 (light pen H) is read-only: a write does nothing");
        set(c, 20, 0x77);
        CHECK(!c.readData(v), "address 20 is no register");
    }

    SECTION("mc6845: horizontal timing -- DE for R1 characters, HSYNC at R2 for R3 (Figure 12)");
    {
        Mc6845 c;
        program(c);
        settle(c);
        bool ok = c.hCount() == 0 && c.vCount() == 0;
        std::vector<bool> de, hs;
        std::vector<uint16_t> ma;
        for (int i = 0; i < 10; ++i) {
            de.push_back(c.de());
            hs.push_back(c.hsync());
            ma.push_back(c.ma());
            c.advance(1);
        }
        for (int i = 0; i < 10; ++i) {
            ok = ok && de[i] == (i < 6);
            ok = ok && hs[i] == (i == 7 || i == 8);
            ok = ok && ma[i] == 0x0100 + i;
        }
        CHECK(ok, "DE at characters 0-5, HSYNC at 7-8, MA = start + character");
        CHECK(c.hCount() == 0 && c.ra() == 1, "the 11th clock starts scan line 1 of the row");
        CHECK(c.ma() == 0x0100, "every scan line of a row starts again at the row's address");
    }

    SECTION("mc6845: R3 = 0 gives no HSYNC");
    {
        Mc6845 c;
        program(c);
        set(c, 3, 0);
        settle(c);
        bool any = false;
        for (int i = 0; i < 10; ++i, c.advance(1)) any = any || c.hsync();
        CHECK(!any, "no HS pulse in the whole line");
    }

    SECTION("mc6845: vertical timing -- rows, the adjust, DE for R6 rows, 16 lines of VSYNC (Fig. 13)");
    {
        Mc6845 c;
        program(c);
        settle(c);
        CHECK(c.fieldLength() == 330, "a field is (R0+1) x ((R4+1)(R9+1) + R5) = 330 clocks");
        const uint64_t f0 = c.fields();
        // Look at character 0 of each of the 33 scan lines, then 7 of the next field.
        std::vector<bool> de, vs;
        std::vector<uint16_t> ma;
        std::vector<uint8_t> ra;
        for (int line = 0; line < 40; ++line) {
            de.push_back(c.de());
            vs.push_back(c.vsync());
            ma.push_back(c.ma());
            ra.push_back(c.ra());
            c.advance(10);
        }
        bool deOk = true, vsOk = true, maOk = true;
        for (int line = 0; line < 33; ++line) {
            deOk = deOk && de[line] == (line < 24);                  // rows 0-7
            const int row = line < 30 ? line / 3 : 10;
            maOk = maOk && ma[line] == 0x0100 + 6 * row;
        }
        // The pulse starts at line 24 and lasts 16 lines: 9 in this field (24-32) and 7 in the
        // next (33-39). The previous field's pulse did the same, so lines 0-6 here carry it.
        for (int line = 0; line < 40; ++line) {
            const bool want = line < 7 || (line >= 24 && line < 24 + 16);
            vsOk = vsOk && vs[line] == want;
        }
        CHECK(deOk, "DE in the first 8 rows (lines 0-23) only");
        CHECK(maOk, "row k starts at start + k x R1; the adjust lines continue after row 9");
        CHECK(vsOk, "VSYNC from row 8 (line 24) for exactly 16 scan lines, into the next field");
        CHECK(ra[30] == 0 && ra[31] == 1 && ra[32] == 2, "the adjust lines count RA 0-2");
        CHECK(ra[33] == 0 && c.fields() == f0 + 1, "line 33 is line 0 of the next field");
    }

    SECTION("mc6845: where the beam is -- lineOfField and clocksUntil");
    {
        Mc6845 c;
        program(c);
        settle(c);
        CHECK(c.clocksUntil(24, 0) == 240, "row 8 starts 240 clocks after the top");
        c.advance(243);
        CHECK(c.lineOfField() == 24 && c.hCount() == 3 && c.vsync(), "...and there VSYNC is on");
        CHECK(c.clocksUntil(24, 3) == 0, "0 when the counters are at the place now");
        CHECK(c.clocksUntil(0, 0) == 330 - 243, "the top of the next field");
    }

    SECTION("mc6845: the cursor -- the address, R10 to R11, DE (Figure 17)");
    {
        // Figure 17's example: cursor register = Nhd + 2, start 1, end 3. Here with 4 lines a row.
        Mc6845 c;
        program(c);
        set(c, 9, 3);
        set(c, 12, 0);
        set(c, 13, 0);
        set(c, 14, 0);
        set(c, 15, 6 + 2);
        set(c, 10, 0x01);    // non-blink, start 1
        set(c, 11, 3);
        c.reset();
        settle(c);
        std::vector<bool> seen;
        for (int clk = 0; clk < 80; ++clk, c.advance(1))
            if (c.cursor()) seen.push_back(true);
        CHECK(seen.size() == 3, "the cursor shows on 3 scan lines (1-3) at one character");
        c.reset();
        settle(c);
        c.advance(40 + 10 + 2);                 // row 1, scan line 1, character 2
        CHECK(c.ma() == 8 && c.ra() == 1 && c.cursor(), "at row 1, character 2, scan line 1");
    }

    SECTION("mc6845: the cursor blink modes (Table 4)");
    {
        Mc6845 c;
        program(c);
        set(c, 14, 0x01);
        set(c, 15, 0x00);
        set(c, 11, 2);
        auto shown = [&](uint8_t r10) {
            set(c, 10, r10);
            return c.cursorAt(0x0100, 0);
        };
        CHECK(shown(0x00), "00: non-blink, always on");
        CHECK(!shown(0x20), "01: cursor non-display");
        // Count the fields on which mode 10 (1/16) and mode 11 (1/32) are on, over 64 fields.
        int on16 = 0, on32 = 0;
        bool first16 = false, ninth16 = true;
        for (int f = 0; f < 64; ++f) {
            const uint64_t at = c.fields();
            if (shown(0x40)) ++on16;
            if (f == 0) first16 = shown(0x40);
            if (f == 8) ninth16 = shown(0x40);
            if (shown(0x60)) ++on32;
            while (c.fields() == at) c.advance(c.fieldLength());
        }
        CHECK(on16 == 32 && on32 == 32, "each blink mode is on for half its period");
        CHECK(first16 && !ninth16, "1/16: 8 fields on, then 8 off");
    }

    SECTION("mc6845: the light pen latches the refresh address + 2 (Figure 5)");
    {
        Mc6845 c;
        program(c);
        settle(c);
        c.advance(30 + 3);                      // row 1, scan line 0, character 3
        const uint16_t m = c.ma();
        c.lightPenStrobe();
        uint8_t h = 0, l = 0;
        c.selectRegister(16);
        c.readData(h);
        c.selectRegister(17);
        c.readData(l);
        CHECK(m == 0x0109, "the beam is at 0100 + 6 + 3");
        CHECK(((h << 8) | l) == m + 2, "R16/R17 read M + 2");
    }

    SECTION("mc6845: RESET -- the counters start again, the registers stay, DE waits a field");
    {
        Mc6845 c;
        program(c);
        settle(c);
        c.advance(123);
        c.reset();
        CHECK(c.hCount() == 0 && c.vCount() == 0 && c.ra() == 0 && c.ma() == 0x0100,
              "the counters are at the top again");
        CHECK(c.reg(0) == 9 && c.reg(4) == 9 && c.startAddress() == 0x0100, "the registers kept");
        CHECK(!c.de(), "no DE in the first field after RESET");
        c.advance(c.fieldLength());
        CHECK(c.de(), "DE once that field has been displayed");
    }

    SECTION("mc6845: coincidence -- R0 written below the count runs the count to its wrap");
    {
        Mc6845 c;
        program(c);
        settle(c);
        c.advance(5);
        set(c, 0, 3);                           // the count is at 5, past the new total
        c.advance(254);
        CHECK(c.hCount() == 3 && c.ra() == 0, "the 8-bit count wraps and runs on to 3");
        c.advance(1);
        CHECK(c.hCount() == 0 && c.ra() == 1, "...where the line ends");
    }

    SECTION("mc6845: interlace sync and video -- the even field takes even lines, the odd odd (14c)");
    {
        Mc6845 c;
        program(c);
        set(c, 8, 3);
        set(c, 9, 3);                           // 4 scan lines a row: 2 in each field
        c.reset();
        auto rasOfRow0 = [&]() {
            std::vector<uint8_t> r;
            for (int i = 0; i < 3; ++i, c.advance(10)) r.push_back(c.ra());
            return r;
        };
        // Go to the top of a field and note its parity.
        settle(c);
        const bool odd = c.fields() & 1;
        const auto a = rasOfRow0();
        c.advance(c.clocksUntil(0, 0));
        const auto b = rasOfRow0();
        const auto& even = odd ? b : a;
        const auto& oddf = odd ? a : b;
        CHECK(even[0] == 0 && even[1] == 2 && even[2] == 0, "even field: RA 0, 2, then row 1 at 0");
        CHECK(oddf[0] == 1 && oddf[1] == 3 && oddf[2] == 1, "odd field: RA 1, 3, then row 1 at 1");
    }

    SECTION("mc6845: a long run skips whole fields and ends where stepping ends");
    {
        Mc6845 a, b;
        program(a);
        program(b);
        a.advance(1000003);
        for (int i = 0; i < 1000; ++i) b.advance(1000);
        b.advance(3);
        CHECK(a.hCount() == b.hCount() && a.vCount() == b.vCount() && a.ra() == b.ra() &&
                  a.fields() == b.fields() && a.vsync() == b.vsync() && a.ma() == b.ma(),
              "one advance of 1000003 clocks = 1000 of 1000 and one of 3");
    }

    SECTION("mc6845: a snapshot carries the registers and the counters");
    {
        Mc6845 a;
        program(a);
        settle(a);
        a.advance(777);
        a.lightPenStrobe();
        a.selectRegister(15);
        StateWriter w;
        a.serialize(w);
        Mc6845 b;
        StateReader r(w.data());
        b.deserialize(r);
        bool same = true;
        for (int i = 0; i < Mc6845::kRegisters; ++i) same = same && a.reg(i) == b.reg(i);
        CHECK(same && b.addressRegister() == 15, "the registers and the address register");
        CHECK(b.hCount() == a.hCount() && b.vCount() == a.vCount() && b.ra() == a.ra() &&
                  b.ma() == a.ma() && b.fields() == a.fields() && b.de() == a.de(),
              "the counters, so the beam is where it was");
    }
}
