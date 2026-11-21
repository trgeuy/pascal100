#include "chips/mc6845.h"

#include "core/statefile.h"

namespace altair {

namespace {

// The width of each register (Table 2, "Number of Bits").
constexpr uint8_t kMask[Mc6845::kRegisters] = {
    0xFF, 0xFF, 0xFF, 0x0F,   // R0 H total, R1 H displayed, R2 H sync position, R3 sync width
    0x7F, 0x1F, 0x7F, 0x7F,   // R4 V total, R5 V total adjust, R6 V displayed, R7 V sync pos.
    0x03, 0x1F, 0x7F, 0x1F,   // R8 interlace, R9 max scan line, R10 cursor start, R11 end
    0x3F, 0xFF, 0x3F, 0xFF,   // R12/R13 start address, R14/R15 cursor
    0x3F, 0xFF,               // R16/R17 light pen
};

constexpr uint8_t kVsyncLines = 16;  // "The vertical sync pulse width is fixed at 16 scan-line times"

} // namespace

void Mc6845::reset() {
    hc_ = 0;
    vc_ = 0;
    adj_ = 0;
    inAdjust_ = false;
    vsLeft_ = 0;
    firstField_ = true;
    ra_ = ((r_[8] & 3) == 3) ? (uint8_t)(fields_ & 1) : 0;
    maRow_ = startAddress();
    vdisp_ = true;
    startRow();
}

void Mc6845::writeData(uint8_t v) {
    if (addr_ < 16) r_[addr_] = v & kMask[addr_];  // R16/R17 are read-only; 18-31 are nothing
}

bool Mc6845::readData(uint8_t& v) const {
    if (addr_ < 14 || addr_ > 17) return false;    // write-only, or no register
    v = r_[addr_];
    return true;
}

bool Mc6845::de() const {
    return !firstField_ && vdisp_ && hc_ < r_[1];
}

bool Mc6845::hsync() const {
    const unsigned w = r_[3] & 0x0F;               // 0: no HS (R3)
    return w != 0 && hc_ >= r_[2] && (unsigned)hc_ < (unsigned)r_[2] + w;
}

bool Mc6845::cursorPhaseOn() const {
    switch ((r_[10] >> 5) & 3) {
    case 0: return true;                           // non-blink
    case 1: return false;                          // cursor non-display
    case 2: return ((fields_ / 8) & 1) == 0;       // blink, 1/16 of the field rate
    default: return ((fields_ / 16) & 1) == 0;     // blink, 1/32 of the field rate
    }
}

bool Mc6845::cursorAt(uint16_t ma, uint8_t ra) const {
    return ma == cursorAddress() && ra >= (r_[10] & 0x1F) && ra <= r_[11] && cursorPhaseOn();
}

bool Mc6845::cursor() const {
    return de() && cursorAt(ma(), ra_);
}

// The end of a character row: the row address equals R9. In interlace sync and video mode the
// row address steps by two, so the row ends on the pair that holds R9 (Figure 14c).
bool Mc6845::rowEnds() const {
    if ((r_[8] & 3) == 3) return (ra_ | 1) == (r_[9] | 1);
    return ra_ == r_[9];
}

uint8_t Mc6845::linesPerRow() const {
    if ((r_[8] & 3) == 3) return (uint8_t)(((r_[9] | 1) + 1) / 2);
    return (uint8_t)(r_[9] + 1);
}

// At the first scan line of each character row: the vertical display ends at row R6, and
// VSYNC starts at row R7 (Figure 13).
void Mc6845::startRow() {
    if (vc_ == r_[6]) vdisp_ = false;
    if (vc_ == r_[7]) vsLeft_ = kVsyncLines;
}

void Mc6845::newField() {
    ++fields_;
    firstField_ = false;
    vc_ = 0;
    adj_ = 0;
    inAdjust_ = false;
    ra_ = ((r_[8] & 3) == 3) ? (uint8_t)(fields_ & 1) : 0;
    maRow_ = startAddress();
    vdisp_ = true;
    startRow();
}

void Mc6845::endLine() {
    hc_ = 0;
    if (vsLeft_) --vsLeft_;
    if (inAdjust_) {
        adj_ = (adj_ + 1) & 0x1F;
        ra_ = adj_;
        if (adj_ == r_[5]) newField();
        return;
    }
    const uint8_t step = ((r_[8] & 3) == 3) ? 2 : 1;
    if (!rowEnds()) {
        ra_ = (ra_ + step) & 0x1F;
        return;
    }
    // The last scan line of the row.
    maRow_ = (uint16_t)((maRow_ + r_[1]) & 0x3FFF);
    if (vc_ == r_[4]) {                      // the vertical total
        if (r_[5] == 0) {
            newField();
        } else {
            inAdjust_ = true;
            adj_ = 0;
            ra_ = 0;
        }
        return;
    }
    vc_ = (vc_ + 1) & 0x7F;
    ra_ = ((r_[8] & 3) == 3) ? (uint8_t)(fields_ & 1) : 0;
    startRow();
}

uint64_t Mc6845::fieldLength() const {
    const uint64_t lines = (uint64_t)(r_[4] + 1) * linesPerRow() + r_[5];
    return (uint64_t)(r_[0] + 1) * (lines ? lines : 1);
}

uint32_t Mc6845::lineOfField() const {
    const uint32_t lpr = linesPerRow();
    if (inAdjust_) return (uint32_t)(r_[4] + 1) * lpr + adj_;
    const uint32_t inRow = ((r_[8] & 3) == 3) ? (uint32_t)(ra_ >> 1) : ra_;
    return (uint32_t)vc_ * lpr + inRow;
}

uint64_t Mc6845::clocksUntil(uint32_t line, uint8_t hc) const {
    const uint64_t len = fieldLength();
    const uint64_t per = (uint64_t)r_[0] + 1;
    const uint64_t now = ((uint64_t)lineOfField() * per + hc_) % len;
    const uint64_t at = ((uint64_t)line * per + hc) % len;
    return (at + len - now) % len;
}

void Mc6845::advance(uint64_t n) {
    while (n) {
        // A long run starts at the top of a field: skip whole fields. Only when one field is
        // exactly like the next -- not in interlace video mode, whose fields differ, and only
        // when VSYNC ends inside the field.
        if (n > 4 * fieldLength() && hc_ == 0 && vc_ == 0 && ra_ == 0 && !inAdjust_ &&
            vsLeft_ == 0 && (r_[8] & 3) != 3) {
            const uint64_t lines = (uint64_t)(r_[4] + 1) * linesPerRow() + r_[5];
            const bool vsInside = r_[7] > r_[4] ||
                                  (uint64_t)r_[7] * linesPerRow() + kVsyncLines <= lines;
            if (vsInside) {
                const uint64_t len = fieldLength();
                const uint64_t k = n / len - 2;   // leave the last two to run, for the pins
                n -= k * len;
                fields_ += k;
                firstField_ = false;
                continue;
            }
        }
        const uint64_t rem = (uint64_t)((uint8_t)(r_[0] - hc_)) + 1;  // clocks left in the line
        if (n >= rem) {
            n -= rem;
            endLine();
        } else {
            hc_ = (uint8_t)(hc_ + n);
            n = 0;
        }
    }
}

void Mc6845::serialize(StateWriter& w) const {
    w.raw(r_, kRegisters);
    w.u8(addr_);
    w.u8(hc_);
    w.u8(ra_);
    w.u8(vc_);
    w.u8(adj_);
    w.boolean(inAdjust_);
    w.u16(maRow_);
    w.u8(vsLeft_);
    w.boolean(vdisp_);
    w.boolean(firstField_);
    w.u64(fields_);
}

void Mc6845::deserialize(StateReader& r) {
    r.raw(r_, kRegisters);
    addr_ = r.u8() & 0x1F;
    hc_ = r.u8();
    ra_ = r.u8() & 0x1F;
    vc_ = r.u8() & 0x7F;
    adj_ = r.u8() & 0x1F;
    inAdjust_ = r.boolean();
    maRow_ = r.u16() & 0x3FFF;
    vsLeft_ = r.u8();
    vdisp_ = r.boolean();
    firstField_ = r.boolean();
    fields_ = r.u64();
}

} // namespace altair
