#pragma once
//
// MC6845 CRTC -- Motorola's CRT controller.
//
// A CHIP, NOT A BOARD. The CompuPro PC Video carries one (DESIGN.md 7.4.1). Modeled from
// the Motorola MC6845 data sheet (23 pp., docs/sources.md), not from a driver that
// programs it.
//
// It knows nothing about S-100, dots or colors. It has a register file, the RS/CS/R/W
// interface, and counters that run on the character clock (CLK). It puts out what its
// pins put out: the refresh address MA0-MA13, the row address RA0-RA4, DE, HSYNC, VSYNC
// and CURSOR. The board turns those into a picture and decides what time is.
//
// TIME IS CHARACTER CLOCKS. The chip has no clock of its own: the board converts its time
// into character clocks and calls advance(). Nothing here reads a host clock.
//
// THE COUNTERS USE COINCIDENCE, as the data sheet says ("Coincidence (CO) circuits
// continuously compare counter contents to the contents of the programmable register
// file"). A counter ends its count when it EQUALS its register. A register written below
// the counter's present value is passed, and the counter runs on to the end of its width
// and wraps: 8 bits horizontal, 7 bits vertical, 5 bits for the row address and the adjust.
//
// LIMITATIONS (each from the data sheet, and each not needed by the PC Video):
// - Interlace sync mode (R8 = 01) counts as non-interlace: the half scan line that delays
//   the odd field's VSYNC (Figure 13) is not modeled. Interlace sync and video mode
//   (R8 = 11) steps the row address by two, even field then odd (Figure 14c), with no half
//   line.
// - HSYNC is a function of the character count within the line (R2 <= count < R2 + R3).
//   A pulse that would run past the end of the line ends with it.
// - Test mode (RESET low with LPSTB high, Table 1) is not modeled.

#include <cstdint>

namespace altair {

class StateReader;
class StateWriter;

class Mc6845 {
public:
    static constexpr int kRegisters = 18;  // R0-R17 (Table 2)

    Mc6845() { reset(); }

    // The RESET pin low then high: the counters clear and the display starts again from the
    // top; the registers keep their values. DE and CURSOR stay inactive until the first frame
    // has been displayed (pp. 9, "Functionality of RESET").
    void reset();

    // ---- The processor interface (CS low; RS picks the register) ----
    // RS = 0, write: the address register (5 bits). There is no RS = 0 read.
    void selectRegister(uint8_t v) { addr_ = v & 0x1F; }
    uint8_t addressRegister() const { return addr_; }
    // RS = 1, write: the register the address register points to. A read-only register
    // (R16, R17) or an address of 18-31 takes nothing.
    void writeData(uint8_t v);
    // RS = 1, read. Only R14-R17 can be read (Table 2). For any other register the chip
    // does not drive the data bus: returns false and leaves `v` alone.
    bool readData(uint8_t& v) const;

    // The register file as stored (masked to each register's width), for SHOW and a renderer.
    uint8_t reg(int r) const { return (r >= 0 && r < kRegisters) ? r_[r] : 0; }

    // ---- Time ----
    // Run the counters `n` character clocks on.
    void advance(uint64_t n);

    // ---- The pins, as they are now ----
    uint16_t ma() const { return (uint16_t)((maRow_ + hc_) & 0x3FFF); }  // MA0-MA13
    uint8_t  ra() const { return ra_; }                                  // RA0-RA4
    bool     de() const;
    bool     hsync() const;
    bool     vsync() const { return vsLeft_ > 0; }
    bool     cursor() const;

    // The LPSTB pin's low-to-high edge: the refresh address goes into R16/R17. The data sheet's
    // Figure 5 gives the address latched for an edge in character M as M + 2.
    void lightPenStrobe() { setLightPen((uint16_t)((ma() + 2) & 0x3FFF)); }

    // ---- What a renderer needs ----
    // The counters, so a board can tell where the beam is.
    uint8_t  hCount() const { return hc_; }
    uint8_t  vCount() const { return vc_; }
    bool     inAdjust() const { return inAdjust_; }
    // Fields started since power-on. The cursor blinks on this count (Table 4); a board that
    // counts VSYNC pulses for its own blink can use it too.
    uint64_t fields() const { return fields_; }
    // R12/R13, R14/R15 as 14-bit addresses.
    uint16_t startAddress() const { return (uint16_t)(((r_[12] & 0x3F) << 8) | r_[13]); }
    uint16_t cursorAddress() const { return (uint16_t)(((r_[14] & 0x3F) << 8) | r_[15]); }
    uint16_t lightPenAddress() const { return (uint16_t)(((r_[16] & 0x3F) << 8) | r_[17]); }
    // The refresh address of the first character of display row `row` (Figure 17: each row
    // starts R1 characters after the one above it).
    uint16_t rowStart(int row) const {
        return (uint16_t)((startAddress() + (uint32_t)row * r_[1]) & 0x3FFF);
    }
    // Is the cursor drawn at refresh address `ma`, row address `ra`, in the present blink phase?
    // (The address match, R10's start and R11's end, and R10 bits 5-6. Not gated by DE.)
    bool cursorAt(uint16_t ma, uint8_t ra) const;
    // The cursor's blink phase now: R10 bits 6-5 = 00 on, 01 never, 10 1/16 of the field rate,
    // 11 1/32 (Table 4). On for the first half of each period.
    bool cursorPhaseOn() const;

    // Character clocks in one field with the registers as they are: (R0 + 1) x
    // ((R4 + 1) x scan lines per row + R5). Never 0.
    uint64_t fieldLength() const;
    // Character clocks from now until the counters reach horizontal count `hc`, scan line
    // `line` of the field (row x scan lines per row + row address, the adjust lines after),
    // with the registers as they are. 0 if they are there now.
    uint64_t clocksUntil(uint32_t line, uint8_t hc) const;
    // The scan line of the field the counters are on now, as above.
    uint32_t lineOfField() const;

    void serialize(StateWriter& w) const;
    void deserialize(StateReader& r);

private:
    void endLine();
    void newField();
    void startRow();
    bool rowEnds() const;
    uint8_t linesPerRow() const;
    void setLightPen(uint16_t a) {
        r_[16] = (uint8_t)((a >> 8) & 0x3F);
        r_[17] = (uint8_t)a;
    }

    uint8_t  r_[kRegisters] = {};
    uint8_t  addr_ = 0;

    // The counters.
    uint8_t  hc_ = 0;          // character in the line (8 bits)
    uint8_t  ra_ = 0;          // row address, the scan line in the row (5 bits)
    uint8_t  vc_ = 0;          // character row (7 bits)
    uint8_t  adj_ = 0;         // scan lines into the vertical total adjust (5 bits)
    bool     inAdjust_ = false;
    uint16_t maRow_ = 0;       // the refresh address of this row's first character
    uint8_t  vsLeft_ = 0;      // scan lines of VSYNC still to come (16 at the start)
    bool     vdisp_ = true;    // the vertical display period: rows 0 to R6 - 1
    bool     firstField_ = true;  // after RESET, DE and CURSOR wait for the first field to end
    uint64_t fields_ = 0;
};

} // namespace altair
