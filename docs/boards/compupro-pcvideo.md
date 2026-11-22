# CompuPro PC Video (`pcvideo`)

**Status:** works. Color and monochrome modes: the decode, the 16K, all ports, the 6845, text
and graphics in a window, wait states, the keyboard (the window's own keys, and the `kbd`
endpoint), the light pen from the mouse. Monochrome (`monitor = mono`, S2-9 ON): the 16.257
MHz clock, 9x14 characters with the 9th dot, underline and reverse, black-and-white graphics,
a green monitor.

## The real hardware

The **PC Video** (board 229D, Viasyn, 1985) is an IEEE 696 / S-100 display board: 16K of
static RAM, a Motorola **6845** CRT controller, a character generator PROM (U6, a 2764), an IBM
PC keyboard port and a light pen port. It copies the IBM **Color Graphics Adapter** and
**Monochrome Display Adapter** at the bus level: the same memory addresses, the same port
addresses and the same register bits. CompuPro's Concurrent DOS 8-16 uses it as console 0, and
its PC Mode needs it: the XIOS sends the PC video and keyboard functions only to this board's
driver.

One switch (S2-9) picks the monitor: color (a 14.31818 MHz master clock) or monochrome
(16.257 MHz). The color modes are 80x25 and 40x25 text, 320x200 graphics in four colors and
640x200 graphics in two. The monochrome modes are 80x25 text in 9x14 cells and 320x200 and
640x200 graphics in black and white (the IBM Monochrome Display Adapter has no graphics).
The switches, not the monitor strap, set the addresses: the IBM monochrome ones are `0B0000`
and `03B0`, or port 1 moves the board there.

| Switch or jumper | What it sets | Default |
|---|---|---|
| S1-1 | Read/write (ON) or write-only memory (OFF) | ON |
| S1-2..9 | Memory: A21-A14 of the 16K block | `0B8000` (color) |
| S2-1..8 | Ports: A11-A4 of the 16 ports | `03D0` (color) |
| S2-9 | Monitor: color (OFF) or monochrome (ON) | |
| S2-10 | Pull PHANTOM\* while the RAM is addressed | OFF |
| J2 | Port 1 (the address flip) works (A-C) or not (B-C) | A-C |
| J3 | The bit of port 0 that enables this board | |
| J4 | The VI line of the keyboard and light pen | VI5 (CompuPro) |
| J5 | The cursor blinks (installed) or is steady | installed |

## Sources

| Source | Path | Authority |
|---|---|---|
| *PC Video Technical Manual* (A493), with the logic diagram | maintainer's copy, `docs/sources.md` | Everything on the board |
| Motorola *MC6845 CRT Controller* data sheet | maintainer's copy, `docs/sources.md` | The 6845 (`src/chips/mc6845.h`) |
| CompuPro XIOS 4.1D source: `VSCPC.A86`, `PCKEY.DVR` | not in the repository | The keyboard's scan codes; a cross-check |

The manual's text names the status buffer U42; the logic diagram (sheet 6) shows U24. The
diagram won.

## Register reference

All ports are decoded on A0-A15. Port numbers are relative to `ports` (default `03D0`).

| Port | OUT (write) | IN (read) |
|---|---|---|
| 0 | Board select: the bit chosen by J3 enables the board. Works while the board is disabled. | `FF` |
| 1 | Address flip, D0: 1 moves the board to the monochrome addresses (A15 of the memory compare, A5-A6 of the port compare inverted) | `FF` |
| 4 | 6845 address register | `FF` |
| 5 | 6845 data register | 6845 R14-R17; `FF` for the write-only registers |
| 8 | Mode, 6 bits: D0 HRES, D1 GRPH, D2 SBW1, D3 VIDENB, D4 SBW2, D5 ENBBLINK | `FF` |
| 9 | Color, 6 bits: D0-D3 border or background (B, G, R, I), D4 graphics intensity, D5 the 320x200 palette | `FF` |
| 10 | — | Status: D0 1 while blanked, D1 keyboard or light pen ready, D2 light pen switch (0 while pressed), D3 vertical sync, D4-D7 1 |
| 11 | Clears the keyboard data, the ready flag and the light pen flag; D7 = 1 holds the keyboard clock | `FF` |
| 12 | Sets the ready flag | The keyboard data (0 after a light pen strobe) |
| 2, 3, 6, 7, 13-15 | Nothing | `FF` |

The screen RAM is 16K: even bytes are characters, odd bytes attributes. In text mode the 6845's
address selects a word; in graphics mode the even scan lines are at offset 0 and the odd ones
at 2000h, 80 bytes a line.

## How it is simulated

- **Decode.** Memory cycles at the 16K block, I/O cycles at the 16 ports, all 16 address bits
  (`decodesIoHigh` names both port blocks the flip can reach). The board answers SIXTN\* on an
  even memory address, except in write-only mode. While the board is disabled it decodes only
  a write to port 0.
- **Time.** The board converts host T-states into master clocks (14.31818 MHz color, 16.257
  MHz mono), and those into the 6845's character clocks: 8 a character for color 80x25 text, 9
  for mono 80x25 text, 16 for 40x25 text and graphics. The status bits, the blink rates, the
  light pen and the wait states all come from that.
- **Wait states.** Each access waits for the next transfer window of the character clock (state
  3 of an 8-clock cycle, states 3-4 of the mono 9-clock cycle, states 4-11 of a 16-clock cycle),
  then 1 master clock, then the transfer: 2 master clocks, or 8 for the 6845. The CPU pays them
  through PRDY.
- **The picture.** `Indexed8`: the 16 colors, and for mono black, green and bright green. One
  dot for each master clock: 640 dots across in every color mode and in mono graphics, 720 in
  mono text, plus a border of 32 dots and 16 lines. The window shows each scan line twice for
  color (200 lines) and 1.5 times for mono (350 lines). The frame is drawn whole from the state
  at that moment. Above the picture, a title bar (`title`, `titlebar`), as a terminal window
  has: one character row of the board's own font, doubled, black on light gray. The guest
  never sees it.
- **Mono text** (Table 31, pp. 61-66). The character comes from the 9x14 half of the PROM
  (A12 = 0). The 9th dot is dark, or a copy of the 8th for C0-DF (the line drawing
  characters). Attributes: AT6-4 = 111 with AT2-0 = 000 is reverse; AT6-4 = 000 with AT2-0 =
  001 underlines raster line 12; AT3 brightens; AT7 blinks (ENBBLINK 1). Any other colour bits
  give a normal character, attribute 00 included. The border is black.
- **The keyboard.** Two sources share one queue of scan codes:
  - The board's window. The board claims physical keys for it (`Display::claimRawKeys`). Each
    key down and up arrives as a USB HID usage, and the board sends the PC/XT code for it
    (`scanCodeFor`). A held key repeats as more key downs. Keys the 83-key keyboard did not
    have go to the key that did their job: the arrows, Home, End, PgUp, PgDn, Insert and
    Delete to the keypad. A paste (Cmd-V, or Ctrl-Shift-V) is typed as ASCII. The chord's
    Ctrl, Shift or Cmd went to the guest as key downs; they are released before the paste, so
    the pasted keys do not arrive with them held.
  - The unit `kbd`. With `keys = ascii` each byte is a character, typed with Shift or Ctrl
    around the key down and the key up. With `keys = scancodes` each byte is a scan code.
    The stream is read only while the queue holds fewer than 256 codes; the rest waits in
    the stream.

  The board takes one code, sets the ready flag and pulls the VI line, and holds the next code
  until the guest writes port 11; the next comes 1 ms later.
- **The light pen.** `lightpen = mouse`: the left button over the window puts the pen at that
  dot and closes its switch. Once each field, when the beam reaches a lit dot under the pen,
  the 6845 latches its address + 2 and the ready flag sets.

| Property | Values | Default |
|---|---|---|
| `memory` | A 16K boundary below `400000` | `0B8000` |
| `ports` | A 16-port boundary below `1000` | `03D0` |
| `wom` | `true`, `false` | `false` |
| `phantom` | `true`, `false` | `false` |
| `monitor` | `color`, `mono` | `color` |
| `select` | 0-7 | 0 |
| `flip` | `true`, `false` | `true` |
| `interrupt` | `none`, `int`, `vi0`-`vi7` | `vi5` |
| `cursor_blink` | `true`, `false` | `true` |
| `font` | `builtin:pcvideo` or a file (binary, Intel HEX, S-records) | `builtin:pcvideo` |
| `lightpen` | `off`, `mouse` | `off` |
| `title` | The title bar's text | empty: `CompuPro PC Video (<id>)` |
| `titlebar` | `true`, `false` | `true` |
| `width` | The window width | auto |
| `kbd` `connect` | An endpoint | `null` |
| `kbd` `keys` | `ascii`, `scancodes` | `ascii` |

### The character generator

`builtin:pcvideo` has the layout of U6 (A12 = S2-9, A11-A4 = the character, A3-A0 = the scan
line) and the glyphs of the Modern DOS font (CC0). It is not CompuPro's PROM. A dump of a real
U6 goes in `font`. See `roms/PCVIDEO/README.md`.

### Reset

- `Reset::PowerOn`: the RAM is cleared, the keyboard queue is emptied, the key and light pen
  deadlines and the 1 ms key spacing start again (POWER also restarts the clock), then as
  `Reset::Bus`.
- `Reset::Bus`: the board is selected (U2 preset), not flipped, the mode and color registers
  are 0, the keyboard is clear and its clock free; the 6845's counters restart and its
  registers keep their values.

## Quirks reproduced

| Quirk | If you get it wrong |
|---|---|
| A 16-bit port decode: `03D4`, not `00D4` | A driver that tests for the board at the wrong address finds it |
| Status D0 is 1 while blanked, D3 while in vertical sync | Software that waits for retrace to write the screen hangs or writes at the wrong time |
| Status D2 is active low | A light pen program sees its switch pressed for ever |
| A read of port 12 does not clear the data; port 11 does, and forces 0 | The keyboard driver reads a key twice, or a light pen strobe looks like a key |
| The keyboard holds its next code until port 11 | A driver loses keys typed quickly |
| Graphics: MA12 = RA0, odd lines at 2000h | Every other line of a graphics screen is wrong |
| AT7 is background intensity with ENBBLINK = 0, blink with ENBBLINK = 1 | Bright backgrounds blink, or blinking text is bright |
| Mono: the 9th dot repeats the 8th only for C0-DF | Box lines have gaps, or every character touches the next |
| Mono: underline only for AT6-4 = 000, AT2-0 = 001, on raster line 12 | Blue (in color terms) text is underlined, or not |
| Mono: a text character is 9 master clocks, and the request window is 2 clocks wide | Wrong field rate (about 50 Hz) and wrong wait states |

## Limitations and deliberate departures

- **The monochrome monitor is green** (an IBM 5151-type P39 phosphor): black, green and bright
  green. Video off is black whatever the intensity line says. The board puts out only B/W and
  intensity; the colour is a model decision.
- **Mono attribute 00 shows the character.** Table 4 calls it "blank character (no output)",
  but the decoder (Table 31, pp. 64-66) has no blank function: the B/W line follows the dots.
  The model follows the circuit. Likewise AT7 with ENBBLINK = 0 goes to the background group,
  which is dark on a mono monitor, so it shows nothing (Table 4: "intensify reverse
  character").
- **Mono lines are drawn 1.5 times** so that 720 x 350 comes out near 4:3; color lines twice.
- **The window's keyboard is the 83-key PC/XT keyboard.** F11, F12, Pause and the GUI (Cmd,
  Windows) keys send nothing; there are no E0-prefixed codes. The board does not light the
  host's Caps Lock or Num Lock lamps, and the host's own lock state is not read.
- **Ctrl-E in the window goes to the guest**, as in every other video window. ATTN works at the
  terminal; the window's close box stops the guest.
- **The palette levels** are the usual RGBI monitor levels, with color 6 as brown. The manual
  names the colors, not the levels.
- **The keyboard's 1 ms** between codes is a model decision, not the manual's.
- **The picture is drawn a whole frame at a time.** A guest that changes the 6845 or the mode
  in the middle of a frame does not get a split picture. Interlace is not drawn.
- **The light pen fires on any non-black dot**, once each field. A real pen's sensitivity
  depends on the phosphor and the brightness.
- **Ports 2, 3, 6, 7, 13-15 and status D4-D7 read 1s.** On the board they read an undriven bus.
- **Up to eight boards behind the board select** work as the hardware does: a write to port
  0 reaches every board by design (`sharesPort`), and only two boards selected at one address
  are contention. CompuPro's XIOS uses one.
- J1 (the horizontal sync delay) and the composite output are not modeled.

## Verification

`tests/test_pcvideo.cpp`: the decode, the board select, the flip, write-only mode, 16-bit
transfers, the status timing over three fields of the 80x25 mode, the wait states, the keyboard
(one code, VI5, port 11, the 1 ms, the clock hold, ASCII with Shift and Ctrl, a key after
POWER, the stream back-pressure), two boards behind one select, the window's
keys (the usage table, the claim, the sink, a paste), text (the PROM,
the attribute, blink, the cursor), 320x200 and 640x200 graphics, mono text (the field rate at
16.257 MHz and 9 clocks, the 9x14 half, the 9th dot, attribute 00, reverse, underline,
intensity, blink, the 9-clock wait states) and mono graphics, the light pen, a frame through a
`NullDisplay`, the straps, reset and a snapshot. `tests/test_mc6845.cpp` covers the 6845.

Period software: CompuPro's Concurrent DOS 4.1 XIOS (4.1D), rebuilt with `PCBRD`, `NPC_BRD = 1`
and `COLOR_PC = 1`, boots on the board as console 0 (2026-11-21). Its keyboard driver takes each
code on VI5, reads port 12 and clears with port 11; its screen driver sets mode 29h, draws the
status line on row 25 in reverse video, and runs the shell, a PC Mode program (INT 21h) and a
SPUZ task. Its four virtual consoles switch with Ctrl + keypad 0-3, and `WINDOW` splits the
screen between them. PC Mode programs use the BIOS INT 10h (cursor, write, teletype, scroll,
read back, graphics mode 4) and INT 16h. Two faults found on the way are in CompuPro's and
DRI's code, not the board: the 4.1D XIOS records 640x200 as mode 20h (fixed locally in the
rebuilt XIOS), and the PC Mode kernel does not hand INT 10h function 13 (read dot) back to the
program.

The same XIOS rebuilt with `MONO_PC = 1` boots on the board with `monitor = mono`,
`memory = 0B0000` and `ports = 03B0` (2026-11-21): the shell, the virtual consoles and the BIOS
calls work, and INT 10h reports mode 07. DOS graphics do not exist on a mono system with this
XIOS: it sends DOS modes 4 and 6 to its color routines, as IBM's monochrome adapter has no
graphics either.

## References

- DESIGN.md §7.4.1, the design.
- `roms/PCVIDEO/README.md`, the character generator.
