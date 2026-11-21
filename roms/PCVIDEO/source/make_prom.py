#!/usr/bin/env python3
"""Build PCVIDEO.BIN, the built-in character generator PROM of the CompuPro PC Video.

Usage:  uv run --with fonttools python3 make_prom.py   (run in this folder)

The PC Video's character generator is U6, an 8K 2764 (manual A493, pp. 42 and 61):

  address = MXSEL (A12) | character code (A11-A4) | scan line RA3-RA0 (A3-A0)

MXSEL is S2 paddle 9: 0 = monochrome (the lower 4K), 1 = color (the upper 4K). Each
character has 16 bytes: 14 scan lines are used in monochrome, 8 in color. A byte is one
scan line, bit 7 the leftmost dot. The board adds the 9th monochrome dot itself (blank,
or a copy of the 8th for C0-DF), so the PROM holds 8 dots.

The glyphs are Modern DOS 20190101.02 by Jayvee Enaguas (CC0 1.0; LICENSE-ModernDOS.txt):
the 9x14 face for monochrome, the 8x8 face for color. The fonts are TrueType outlines
drawn from whole pixels, so the center of each pixel is sampled.
"""
from fontTools.ttLib import TTFont
from fontTools.pens.pointInsidePen import PointInsidePen

# CP437 -> Unicode. 01-1F and 7F are the IBM symbols; 00 is blank.
CP437 = [None, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022, 0x25D8, 0x25CB,
         0x25D9, 0x2642, 0x2640, 0x266A, 0x266B, 0x263C, 0x25BA, 0x25C4, 0x2195, 0x203C,
         0x00B6, 0x00A7, 0x25AC, 0x21A8, 0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194,
         0x25B2, 0x25BC]
CP437 += list(range(0x20, 0x7F)) + [0x2302]
CP437 += [ord(c) for c in bytes(range(0x80, 0x100)).decode('cp437')]


def raster(path, width, height):
    """256 glyphs, each a list of `height` rows of `width` dots (bit width-1 = leftmost)."""
    font = TTFont(path)
    cmap, glyphs = font.getBestCmap(), font.getGlyphSet()
    asc, desc = font['hhea'].ascent, font['hhea'].descent
    sx, sy = glyphs[cmap[0x41]].width / width, (asc - desc) / height
    out = []
    for code in range(256):
        rows = []
        for r in range(height):
            bits = 0
            if CP437[code] is not None:
                g = glyphs[cmap[CP437[code]]]
                for c in range(width):
                    pen = PointInsidePen(glyphs, ((c + .5) * sx, asc - (r + .5) * sy))
                    g.draw(pen)
                    if pen.getResult():
                        bits |= 1 << (width - 1 - c)
            rows.append(bits)
        out.append(rows)
    return out


def main():
    prom = bytearray(8192)
    mono = raster('ModernDOS9x14.ttf', 9, 14)
    odd = []
    for code, rows in enumerate(mono):
        for r, bits in enumerate(rows):
            ninth, eighth = bits & 1, (bits >> 1) & 1
            want = eighth if 0xC0 <= code <= 0xDF else 0
            if ninth != want:
                odd.append(code)
            prom[(code << 4) | r] = bits >> 1          # the 8 PROM dots
    color = raster('ModernDOS8x8.ttf', 8, 8)
    for code, rows in enumerate(color):
        for r, bits in enumerate(rows):
            prom[0x1000 | (code << 4) | r] = bits
    open('../PCVIDEO.BIN', 'wb').write(prom)
    odd = sorted(set(odd))
    print('PCVIDEO.BIN written, 8192 bytes')
    print('9x14 glyphs whose 9th column differs from what the board makes:',
          ' '.join('%02X' % c for c in odd) or 'none')


if __name__ == '__main__':
    main()
