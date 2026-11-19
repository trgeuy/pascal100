# CompuPro M-Drive/H — 512K solid state disk emulator

Source: [CompuPro M-Drive:H Manual.pdf](#) (CompuPro, a Godbout company; *M-Drive/H Technical
Manual*, board 197B, document #11978, © 1982, first printing December 1982, second printing
March 1983; 21 scanned pages)

The **CompuPro M-Drive/H** is an IEEE 696 / S-100 board with **512K bytes of dynamic RAM that the
system uses as a disk drive** (drive M:). It takes no memory space: it is **two I/O ports**, one
that loads a starting address into a 22-bit counter and one that reads or writes the byte at that
address. The counter counts up by one after each data byte, so a 128-byte sector moves with one
address load and 128 `IN`s or `OUT`s. **Up to eight boards share the same two ports**; the top
three bits of the counter select the board, so the software sees one contiguous array of up to
4 megabytes. The RAM is lost at power-off. CompuPro's CP/M 2.2, CP/M-86 and MP/M 8-16 have the
driver built in, size the boards at cold boot and format drive M:.

This is a distilled emulation reference: the port map, the S1 switch, the address counter and how
it loads, board selection, the software's track/sector layout and parity scheme, and the sizing
method of CompuPro's own BIOS. The "up in 5 minutes" section, the table of MOVCPM sizes and the
warranty page are summarized only where they carry a fact. Emulated as `mdriveh`
(`docs/boards/compupro-mdriveh.md`).

Page numbers below are the **printed** page numbers (printed = PDF page + 1). The contents page
lists "Service and limited warranty information ... 24"; the scan has the warranty page as its
last page, and printed pages 22-23 are not in the scan (probably blank).

---

## 1. Quick reference for emulation

| Item | Value | Source |
|---|---|---|
| Ports | 2: **data = base + 0** (read/write), **address = base + 1** (write) | pp. 7, 11 (driver: `HDATA EQU HBASE`, `HADDR EQU HBASE+1`) |
| Default base | **C6H** (data C6H, address C7H) | pp. 5, 8 |
| Base select | S1 positions 1-7 = A7-A1, OFF = 1, ON = 0; A0 picks the port | p. 8 table, schematic p. 17 (U18 25LS2521) |
| Address counter | **22 bits** (MA21-MA0), six 74LS161/163 counters | p. 7, schematic p. 17 |
| Address load | each `OUT` to the address port **shifts the counter up 8 bits** and puts the byte in MA7-MA0. Three `OUT`s, high byte first, give a full address. | schematic p. 17 (see §3); driver `SETMDRV` p. 15-16 |
| Data transfer | `IN` data port = the byte at the counter; `OUT` = write it. The counter then counts up by 1. | p. 7 |
| Board select | MA21-MA19 compared with S1 positions 8-10 (ON = 0). The selected board drives data. | pp. 6, 8-9, schematic p. 17 (U17 74LS266) |
| Size | 512K per board, 1 to 8 boards = 512K to 4M | pp. 1, 7 |
| Memory | 64 x 4864 (64K x 1) DRAM in 8 rows, Intel 8203-3 controller, 24 MHz | pp. 18, 20, 21 |
| Power-off | contents lost; no battery | pp. 6, 9 |
| Interrupts, DMA | none | schematic |
| Wait states | RDY (pin 72) held while the 8203 is busy (XACK*) | schematic p. 19 |

---

## 2. The S1 switch

There is one 10-position DIP switch, S1. "Markings on switch, **not** legend": the position
numbers are the ones printed on the switch body.

### 2.1 Positions 1-7: the port pair (pp. 5, 8)

| S1 position | Address bit |
|---|---|
| 1 | A7 |
| 2 | A6 |
| 3 | A5 |
| 4 | A4 |
| 5 | A3 |
| 6 | A2 |
| 7 | A1 |

OFF = "1", ON = "0". A0 is not compared: it picks data (A0 = 0) or address (A0 = 1). So the pair
always starts at an **even** port.

Standard setting, C6H/C7H = 1100 011x: positions 1, 2, 6, 7 OFF; 3, 4, 5 ON.

### 2.2 Positions 8-10: the cascade address (pp. 6, 8-9)

Each board in the system gets a different cascade address. ON = 0:

| Board | S1-8 | S1-9 | S1-10 | Cascade (MA21 MA20 MA19) |
|---|---|---|---|---|
| 1st | ON | ON | ON | 0 |
| 2nd | ON | ON | OFF | 1 |
| 3rd | ON | OFF | ON | 2 |
| 4th | ON | OFF | OFF | 3 |
| 5th | OFF | ON | ON | 4 |
| 6th | OFF | ON | OFF | 5 |
| 7th | OFF | OFF | ON | 6 |
| 8th | OFF | OFF | OFF | 7 |

The schematic (p. 17) pairs S1-8 with MA21, S1-9 with MA20 and S1-10 with MA19 through the three
gates of U17 (74LS266, exclusive-NOR, open collector, wired together as BSEL). The table above and
the schematic agree: the 2nd board has only S1-10 OFF and answers for MA19 = 1.

"These are the only switch settings that are required on the M-Drive/H board." (p. 9)

---

## 3. The address counter (schematic p. 17)

Six 4-bit counters (74LS161/163) hold MA21-MA0 (U4 holds only MA21-MA20; its top two outputs are
not connected). They share one LOAD* and one COUNT* line from the port PAL U6.

The parallel inputs of each counter are wired to the **outputs eight bits below**:

| Counter | Loads from | Gives |
|---|---|---|
| U16 | DB3-DB0 (the data bus) | MA3-MA0 |
| U15 | DB7-DB4 | MA7-MA4 |
| U9 | MA3-MA0 | MA11-MA8 |
| U8 | MA7-MA4 | MA15-MA12 |
| U5 | MA11-MA8 | MA19-MA16 |
| U4 | MA13-MA12 | MA21-MA20 |

So one LOAD pulse (an `OUT` to the address port) does **counter = (counter << 8 | byte) mod 2^22**.
Three `OUT`s, high byte first, load a full 22-bit address; the top two bits of the first byte are
lost. This is why the driver sends A, then H, then L (§5).

COUNT* clocks all six counters as one 22-bit counter (ripple carry through the T and P inputs).
The carry out of MA21 is not connected, so the counter wraps from 3FFFFFH to 0.

---

## 4. Data port and board select

`IN` from the data port returns the byte at the counter; `OUT` writes the data bus to that byte.
After each transfer the counter counts up by one. The read data goes through U19 (74LS374, latched
on XACK*) to DI0-DI7; the write data comes from DO0-DO7 through U13 (74LS244).

BSEL is true when MA21-MA19 equal the board's S1-8..10. Only the selected board drives the read
data (OE*) and starts a DRAM cycle (PCS*). With **no** board selected, nothing drives the bus,
and an `IN` reads the floating bus, FFH. CompuPro's sizing routine depends on this (§6.3).

⚠ **Inferred, not stated: every board loads and counts, selected or not.** The port PAL U6
(197P-1) has BSEL as an input, but its equations are not in the manual. The design needs all
boards to load the same address and to count together: the software sees one contiguous array,
and a transfer that crosses a 512K boundary must continue on the next board. So LOAD* and COUNT*
are taken to be common to all boards, and only OE*/PCS* depend on BSEL.

⚠ **Inferred: the address port is write-only.** No routine in the manual reads port base + 1.

Wait states: U14 (74LS74) sets on pSYNC and drives RDY (pin 72) low through U20 (74LS125) while
PCS* is active, until the 8203's XACK* clears it (schematic p. 19).

PWRFAIL* (pin 13) goes to U6 through jumper J1 (p. 17). Its function is not described.

---

## 5. The software's layout: tracks, sectors, parity

From the sample BIOS on pp. 11-16 ("Written L. Ott 12/17/82"). It treats the M-Drive/H as a disk
with **128-byte sectors**:

- Byte address = `(sector - 1) * 65536 + track * 128`. The driver's `SETMDRV` sends
  A = sector - 1, then H, then L of HL = track * 128.
- So one "sector" is a 64K slice of the array, and a "track" is a 128-byte record inside it:
  512 tracks per sector, **8 sectors per board**.
- One board: 8 sectors per track. More boards: 8 per board (the DPB's SPT "MUST BE INCREASED FOR
  MORE BOARDS").

**Parity.** After the 128 data bytes the driver sends or checks one more byte: an 8-bit sum that
starts at 128 (`MVI C,128` / `MOV B,C`, p. 14) and adds each data byte. It is kept at `(sector - 1) * 65536 + track`, inside the first 512 bytes of each
64K slice. Those are tracks 0-3, which is why the DPB **always reserves 4 tracks** ("NUMBER OF
RESERVED TRACKS FOR PARITY CHECK THIS IS ALWAYS 4"). The board has no parity hardware: this is
software only.

A read retries up to `MRTRY` = 10 times on a parity error.

**The sample DPB** (one board):

| Field | Value |
|---|---|
| (byte before the DPB) | 128 — "MDRIVE TYPE FOR MFORM, DO NOT CHANGE" |
| SPT | 8 |
| BSH, BLM, EXM | 4, 15, 1 (2K blocks) |
| DSM | `((512-4)*8)/16 - 1` = 253 |
| DRM | 127 (128 directory entries) |
| AL0, AL1 | 11000000B, 00000000B |
| CKS | 0 (not removable) |
| OFF | 4 |

---

## 6. CompuPro's software

### 6.1 Operating systems with the driver built in (p. 5)

| System | Revision with M-Drive/H support |
|---|---|
| CP/M 2.2 (CompuPro) | 2.2MDB or later |
| CP/M-86 | 1.1PAA or later |
| MP/M 8-16 | 2.1D or later |

At boot the system finds the boards, prints the size in K, and formats drive M: (pp. 6, 9).
`MFORM M` formats it again by hand and asks first (p. 9).

### 6.2 MDRIVE.COM for other CP/M systems (p. 10)

`MDRIVE -<drive> <address>` sizes the boards, patches the BIOS, puts the driver (about 1K) at
`<address>` and formats the drive. The address is the top of memory minus 1K: FC00 for a 63K
CP/M in 64K, down to 5C00 for 23K in 24K (Table 1).

### 6.3 How CompuPro's CP/M 2.2 BIOS sizes the boards

Not in the manual. From the BIOS source text left in unused sectors of
`tests/media/compupro/CPM22N.DSK` (CompuPro CP/M 2.2N), routine `HMDTEST`:

1. Start with 0 boards and the first board's sector number.
2. **Read** a sector of the board under test, with the parity check.
3. **Write** it back (the write makes a correct parity byte).
4. **Read** it again. No parity error: the board is there. Add 8 to the sector count, add
   (512-4)/2 blocks to the disk size, and go to the next board, up to 8 (64 sectors).
5. A parity error ends the count.

An absent board reads FFH for every byte. With the sample driver's sum (start at 128, add 128 x
FFH) the expected parity is 00H, and the parity byte reads FFH, so the check fails. (CP/M 2.2N's
own `HMD$FNL` was not recovered whole; its sum is taken to be the same, because the manual says
its sample code "was taken out of the standard CompuPro CPM 2.2 BIOS", p. 10.) The write in step 3 matters for a present board whose RAM holds random bits at
power-up: the first read can fail, the second cannot.

With no board, CP/M 2.2N signs on with `0 M-Drive/H`, and its `MFORM M` reports
`DRIVE SPECIFIED IS NOT A MEMORY DRIVE`. CP/M-86 1.1 and MP/M 8-16 print
`NNNK M-DRIVE/H Active` (CP/M-86: `... Active as Drive "M".`) or `NO M-DRIVE/H Active`.

---

## 7. Hardware

| Ref | Part | Job |
|---|---|---|
| U1-U3 | 7805 | +5 V regulators |
| U4, U5, U8, U9, U15, U16 | 74LS161/163 | the 22-bit address counter |
| U6 | 197P-1 (10L8 PAL) | port and cycle decode: OE*, LOAD*, COUNT*, IE*, PCS*, RD*, WR* |
| U7 | 74LS375 | latches sINP, sOUT, A0 and the port match |
| U10 | 197P-2 (10L8 PAL) | MA18-MA16 to RAS0*-RAS7* (row select) |
| U11 | 8203-3 | DRAM controller, refresh, arbitration |
| U12 | 74F04 | 24 MHz oscillator (X1, L1 0.68 µH) |
| U13 | 74LS244 | write data in (DO0-DO7) |
| U14 | 74LS74 | RDY wait-state flip-flop |
| U17 | 74LS266 | cascade compare (MA21-MA19 against S1-8..10) |
| U18 | 25LS2521 | port compare (A7-A1 against S1-1..7) |
| U19 | 74LS374 | read data out (DI0-DI7) |
| U20 | 74LS125 | RDY driver |
| U21-U84 | 4864 or equivalent | 64 x 64K x 1 DRAM, 8 rows of 8 bits |

Parts list p. 21 says "U21-84"; the board silkscreen says "U20 THROUGH U83"; U20 is the 74LS125.

---

## Quirks worth carrying

- ⚠ **The address port shifts.** Each `OUT` moves the counter up 8 bits. Software sends three
  bytes, high first; a fourth `OUT` would push the first byte out. Nothing resets the counter but
  a load.
- ⚠ **Every board loads and counts; only the selected board drives the data lines.** Inferred
  from the design (§4): the PAL equations are not published.
- ⚠ **Absent boards read FFH** (the floating bus), and CompuPro's sizing depends on it (§6.3).
  A model must not return any other value for an unselected board.
- ⚠ **Parity is software.** The board stores 8 bits per byte and nothing more. The 4 reserved
  tracks hold the software's parity bytes.
- ⚠ **Random RAM at power-up.** The BIOS writes before it trusts a read, and formats drive M: at
  cold boot.
- ⚠ **The address port is write-only** (inferred, §4).
- The S1 position numbers are the ones on the switch body, not the PCB legend.
