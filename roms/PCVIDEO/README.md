# PCVIDEO — CompuPro PC Video character generator (`builtin:pcvideo`)

The character generator PROM of the **CompuPro (Viasyn) PC Video** display board, U6. The
part is a 2764 (8 KB). This image is **not a dump of CompuPro's PROM**: we do not have one.
It has the same layout, with the glyphs of a public-domain font. A dump of a real U6, given
as a file path, takes its place.

- **Decoded image:** `0000`–`1FFF`, 8192 bytes, CRC32 `6555EFCD`.
- **Glyphs:** *Modern DOS* version 20190101.02 by Jayvee Enaguas, released under Creative
  Commons Zero 1.0 (public domain). The license is in `source/LICENSE-ModernDOS.txt`. The font
  is based on the IBM VGA and Verite ROM fonts: some glyphs match them and others are redrawn.
  It has all 256 characters of code page 437.

## Layout

From the *PC Video Technical Manual* (A493), pp. 42 and 61:

| PROM address | Signal | What it selects |
|---|---|---|
| A12 | MXSEL (S2 paddle 9) | `0` = monochrome (`0000`–`0FFF`), `1` = color (`1000`–`1FFF`) |
| A11–A4 | The character byte | The character, `00`–`FF` |
| A3–A0 | RA3–RA0 from the 6845 | The scan line, `0`–`F` |

Each character has 16 bytes, one for each scan line. Monochrome uses 14 scan lines, color uses
8. The rest are `00`. Bit 7 is the leftmost dot.

- **Monochrome** is the 9x14 face. The PROM holds 8 dots. The board makes the 9th dot itself: a
  blank, or a copy of the 8th dot for the line-drawing characters `C0`–`DF` (manual p. 62).
  The 9th column of the 9x14 face follows the same rule for all 256 characters, so no dot is
  lost.
- **Color** is the 8x8 face.

## Rebuild the image

The fonts are TrueType outlines drawn from whole pixels. `source/make_prom.py` samples the
center of each pixel:

```sh
cd roms/PCVIDEO/source
uv run --with fonttools python3 make_prom.py
```

The build embeds only the top of the folder, so the files in `source/` are not embedded.

## Use a real PROM

Give the board a dump of its U6 as a file (a 2764 image: binary, Intel HEX or S-records). The
image must use the layout above.
