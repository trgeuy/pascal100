# CompuPro (Viasyn) SPUZ Technical Manual

Source: [CompuPro SPUZ Manual.pdf](#) (provenance in `docs/sources.md`). "SPUZ Technical Manual", Viasyn
Corporation, Hayward CA. Copyright 1985. **First edition April 1985, latest printing July 1985.**
Part No. 8261-0030, file name SPUZNEW.MAN, **Board No. 257E**. Back cover code 385300/2.

43 PDF pages: a scan with an OCR text layer. PDF pages 1-4 are the cover, a photo page, the
copyright page and the contents, so **the PDF page is the printed page + 4**. Printed pages 15-27
(PDF 19-31) are Signetics' SCN2681 data sheet, reprinted. PDF 39 is an addendum (ECO 138), PDF
41 the component layout, PDF 42 the warranty and PDF 43 the back cover.
Converted 2026-11-20. Every converted page was read from the OCR text and checked against the page
image; the image wins. The logic diagram (PDF 34-38) is indexed: sheets 1 and 4 are described from
the image, sheets 2, 3 and 5 are not transcribed.

Conventions are the same as the other CompuPro conversions in this folder. Errors in the printed
manual are marked **[sic]**. Notes that interpret the manual say so.

## Front matter (PDF pages 1-4)

- Cover: CompuPro logo, "SPUZ".
- Copyright page: the facts in "Source" above. Disclaimer; registered trademarks CompuPro, Viasyn
  Corporation, CP/M-86 (Digital Research); compound trademarks Concurrent DOS 8-16, CP/M 8-16;
  trademark SPUZ (Viasyn).
- Contents:

| Section | Page |
|---|---|
| About the SPUZ | 1 |
| How to get your SPUZ up and running without reading much of the manual | 2-3 |
| Hardware section: switches 4-5, jumpers 6-7 | 4-7 |
| Theory of operation: overview 8, the circuitry 8-13, the DUART 14-27 | 8-27 |
| Software section | 28-29 |
| Hardware description: logic diagram 30-34, parts list 36, component layout 37 | 30-37 |

## About the SPUZ (page 1)

The SPUZ combines an **8 MHz Z80H**, **64K or 256K of memory** and **two RS-232C serial channels**
to do "slave processing". Each user has their own processor, memory and serial ports, so the load
on the host is smaller while the slave runs an 8-bit task. With 16- or 32-bit programs the SPUZ
can be a front-end I/O processor that adds 8-bit capability "(as with CompuPro 816/D1, E1, F1, G1
and 286 computers)". **Up to 16 SPUZs can be in one mainframe.** Host and slaves run 8- and 16-bit
software at the same time: "true multi-processing instead of dual processing".

Features: 8 MHz Z80H; 64K or 256K bytes of RAM, and **up to 16 SPUZ boards occupy only 64K of
system memory**; two bi-directional RS-232C serial ports with software-selectable parameters;
supported by CompuPro's Concurrent DOS 8-16; an 8-bit node in a "processor-per-user" system.

## Quick start (pages 2-3)

"How to get your SPUZ up and running without reading much of the manual" sets the board up to run
under **Concurrent DOS 8-16**:

1. Unpack: an extra jumper shunt and two card ears are in the bag.
2. Install the card ears (figure 1, component side).
3. Set the switches as in the hardware section (figure 2: jumper and switch locations).
4. Set the jumper shunts as in the hardware section (notch side up).
5. Insert the board in a slot close to the back of the enclosure, component side to the front.

## Hardware section (pages 4-7)

### Switches

In S1 only positions 4-8 are used; in S2 all positions are used. **A paddle called "0" is ON, "1"
is OFF.**

| SW | Paddle | Function |
|---|---|---|
| 1 | 4 | Z80 M1 wait state |
| 1 | 5 | Board select address bit D3 |
| 1 | 6 | Board select address bit D2 |
| 1 | 7 | Board select address bit D1 |
| 1 | 8 | Board select address bit D0 |
| 2 | 1-8 | Memory address bits A23-A16 (paddle 1 = A23, paddle 8 = A16) |

**SW1 positions 5-8: the 4-bit select address.** Up to 16 SPUZ boards can be in one system, with
their memory all at the same 64K page in system memory and their **18 I/O ports all overlapping at
FC30h-FC41h**. Each board has a different select address. To talk to a board, write its select
address in the lower nibble to the **SLAVESEL port FC40h**; then that board's DUART and memory are
visible and the others are invisible. "It is important to always start your communication to a
particular SPUZ with a write of the correct select address to the SLAVESEL port FC40h." Number the
boards from 0:

| Board | SW1 paddles |
|---|---|
| 0 | 5, 6, 7, 8 ON |
| 1 | 5, 6, 7 ON, 8 OFF |
| 2 | 5, 6, 8 ON, 7 OFF |
| 3 | 5, 6 ON, 7, 8 OFF |
| 4 | 5, 7, 8 ON, 6 OFF |

**SW1 position 4: the Z80 M1 wait state.** Must be ON at 6 MHz and above; may be OFF at 5 MHz and
below. All standard boards are 8 MHz: leave it **ON**.

**SW2: where the 64K window of SPUZ memory appears** to the host and to DMA devices: any of 256
pages with 24-bit addressing, any of 16 with 20-bit addressing. In standard CompuPro systems all
SPUZ boards in a mainframe appear in **one** page. A 256K board also appears as a 64K block, and
64K and 256K boards can be mixed. **The CompuPro standard is page 0Eh** (0E0000h-0EFFFFh): SW2
paddles 1, 2, 3, 4, 8 ON; paddles 5, 6, 7 OFF.

### Jumpers

J1 is 6 pins (shunt across A, B or C; shipped with a solder-side trace across B). J2 is 16 pins
(one shunt at one of 8 positions; shipped with pins and a shunt across VI0). J3 is 3 pins (A-C
right, B-C left; shipped with a solder-side trace across A-C).

| J | Position | Function |
|---|---|---|
| 1 | A | One S-100 bus wait state |
| 1 | B | **Two S-100 bus wait states (as shipped)** |
| 1 | C | Three S-100 bus wait states |
| 2 | VI7 ... VI0 | Z80 interrupt output to VI7\* ... VI0\*; **VI0 as shipped** |
| 3 | A-C | **Four-cycle hold off (as shipped)** |
| 3 | B-C | Eight-cycle hold off |

- **J1** sets the wait states the SPUZ adds to S-100 accesses of its memory or ports. The time to
  read or write SPUZ memory is fixed, so the count depends only on the host's speed: A only at
  6 MHz or less, B at 12 MHz or less, C above 12 MHz. Leave B for all CompuPro systems.
- **J2** picks the VI line the SPUZ can assert. "The Z80 can assert an interrupt to the host by
  writing a bit pattern to a particular port. The host can clear the interrupt by writing a
  different bit pattern to a particular port." **VI0 is the CompuPro standard for all SPUZ
  boards.**
- **J3** is the bus-acknowledge hold off. A host or DMA access issues BUSREQ to the Z80, and the
  Z80 later gives up its internal bus with BUSACK. The Z80 is kept in BUSACK for a while after the
  last access, so a run of accesses (loading a program) waits less. More hold off: less host wait,
  less Z80 throughput. 4 bus cycles is the recommended compromise; 8 by cutting A-C and
  jumpering B-C.

## Theory of operation (pages 8-13)

### Overview (page 8)

"The SPUZ looks like 64K bytes of memory and 18 I/O ports to the S-100 host processor or temporary
bus master (DMA device). **It does not become a temporary master** and is therefore restricted from
accessing other user's memory spaces or system peripherals. The S-100 bus has complete access
rights to all of the SPUZ's resources including all 64K/256K bytes of memory, both bi-directional
serial ports and all internal control ports."

Inter-processor communication: a **bi-directional interrupt structure**; message passing through
the SPUZ's memory; DMA devices can transfer data directly to or from SPUZ memory.

**There is no EPROM on the SPUZ.** At power-up the Z80 is held in reset until the host has put a
program in SPUZ memory and lets the Z80 run. The host can reset the SPUZ at any time.

### The circuitry (pages 8-13)

- **Clock:** a 16 MHz crystal oscillator (X1, U11, L2, R2, C12) divided by two (U12) gives a
  symmetrical 8 MHz; a booster (U11, L1, R1, Q1) drives the Z80 clock. U12 also makes SYNCACK\*,
  BUSACK\* delayed by part of a clock, which starts an external access after the S-100 addresses
  on the internal bus have settled.
- **M1 wait:** above 5 MHz the Z80 needs one M1 wait state so the DRAM has data ready (U24
  generates WAIT\* in M1 cycles; S1-4).
- **Refresh:** the DRAMs need 256 refresh addresses, the Z80 gives 128. U15, U34, U24 and counter
  U32 toggle ZA7 every 128 refresh cycles.
- **The DUART:** the 2681 is mapped into **16 ports of the Z80's I/O space, 0h-Fh, repeated**
  (port 10h = port 0h).
- **Data bus:** buffered by U39 or U40 by direction. DIEN\* on pDBIN, DOEN\* on BUSACK\*. Part of
  the DO bus is buffered by U38 to the board select comparator (U27) and to PTLATCH (U28).
- **I/O decode:** the S-100 address bus is buffered onto the internal bus on BUSACK\* (U26, U37).
  PAL U25 (p372), U36 and U35 decode **S-100 ports FC30h-FC41h**. **FC40h and FC41h are not on the
  internal bus** and can be written at any time. **FC30h-FC3Fh are on the SPUZ bus**: host access
  to them (or to SPUZ memory) is arbitrated by the Z80, so **the SPUZ must not be in reset when
  they are accessed, or the S-100 bus hangs**: the board asserts pRDY waiting for a BUSACK\* the
  reset Z80 never gives.
- **SLAVESEL\*** (write FC40h) is generated on **every** SPUZ when FC40h is written. Only the board
  whose SW1 positions 5-8 match the lower four bits of the DO bus keeps **VPROC** asserted after
  the write. **PTLATCH\*** (write FC41h) is generated only on the board with VPROC. With VPROC, an
  I/O read or write of FC30h-FC3Fh asserts INTI/O\* and reaches the DUART.

S-100 port map (page 9):

| Port | Function |
|---|---|
| FC30h-FC3Fh | DUART: maps to DUART ports 0h-Fh. Serial channels and output bits (read/write) |
| FC40h | SLAVESEL\*: selects which board of a bank of 16 is selected (write only) |
| FC41h | PTLATCH\*: RESET, which board is the valid DMA board, which 64K bank the S-100 bus accesses (write only) |

- **Memory window (page 10):** when A16-A23 match S2 and VBD\* is asserted, comparator U33 asserts
  VMA\*; the host can then access SPUZ memory with the proper sMEMR / sWO\* / sOUT status and pDBIN
  or pWR\*. To access a location (Z80 running or halted, **not reset**): write SLAVESEL to clock in
  VPROC on the trailing edge of pWR\*, then access the 64K window.
- **PTLATCH bits (page 10, U28, write only, host only):**

| Bit | Name | Function |
|---|---|---|
| D3 | ZRST\* | 1 lets the Z80 run; 0 resets the Z80 |
| D2 | VDMA\* | 1 sets this board as the DMA board; 0 turns it off |
| D1 | PB1 | Processor bank 1: high bit of the S-100 bank |
| D0 | PB0 | Processor bank 0: low bit of the S-100 bank |

- **Banks (pages 10-11):** with 256K the Z80, the host and a DMA device can each see a different
  64K page at the same time: **PB0/PB1** (PTLATCH) for the host, **DB0/DB1** (2681 OP2/OP3) for DMA,
  **ZB0/ZB1** (2681 OP4/OP5) for the Z80. With 64K these bits do not matter. Multiplexers U29/U19
  pick ZB if BUSACK\* is not asserted, PB if BUSACK\* is asserted and pHLDA is not, DB if both are.
  U19 also picks VPROC\* (no pHLDA) or VDMA\* (pHLDA) for VBD\*, so the host and a DMA device can
  address different SPUZ boards.
- **DMA set-up (pages 10-11):** select the board with SLAVESEL; write 04h (ORed with ZRST, PB1,
  PB0) to its PTLATCH to assert VDMA\*; write DB0/DB1 to its 2681. DMA then goes to that board and
  page whatever later SLAVESEL writes do. **Warning: only one SPUZ may have VDMA\* asserted at a
  time; permanent damage may result.** The SPUZ does not promise to give up its internal bus in any
  set time, and it locks out the bus for several microseconds every eight consecutive accesses to
  run a refresh cycle.
- **The 2681 (page 11)** does serial receive/transmit, output bits, input bits and the
  counter/timer. **All OPR bits are low on reset**; the remarks in parentheses say what that means
  to the circuitry.

Output bits (page 12):

| Bit | Name | Function (state at reset) |
|---|---|---|
| OP7 | ZINT\* | OP7 and INTRN of the 2681 are tied together to Z80 ZINT\* (interrupt not asserted to Z80). **See the addendum: OP7 is disconnected on 257D/E and later.** |
| OP6 | S100 VI | S-100 vectored interrupt output (**interrupt asserted to S-100 bus**) |
| OP5, OP4 | ZB1, ZB0 | Which 64K bank the Z80 sees with 256K parts (**Z80 starts running from bank 3, the high bank**) |
| OP3, OP2 | DB1, DB0 | Which 64K bank DMA sees with 256K parts, on the valid DMA board (page 3 accessed on reset, the high bank) |
| OP1 | RTSB | Request to send, channel B (inhibits the terminal from transmitting to the DUART) |
| OP0 | RTSA | Request to send, channel A (the same) |

Input bits (page 12):

| Bit | Name | Function |
|---|---|---|
| IP6 | HALT\* | From the Z80 HALT\* pin: the host can see when the Z80 is halted |
| IP5, IP4 | SB1, SB0 | From the bits selecting the page the host accesses or last accessed |
| IP3, IP2 | (OP3, OP2) | Not used and not connected |
| IP1 | CTSB | Clear to send, channel B; low when the device can take characters |
| IP0 | CTSA | Clear to send, channel A; the same |

- **Host access sequence (pages 12-13), PAL U22 (p258):** (1) host or DMA requests a resource
  (memory or port); (2) the SPUZ puts it in a wait state (pRDY through C11/R5) and asserts BREQ\*
  to the Z80; (3) the Z80 gives BUSACK\*, which starts counter U21 and puts data and address on the
  internal bus; (4) SYNCACK\* asserts MRQ\*, IORQ\*, RD\* or WR\* by the input status; (5) the DRAM or
  DUART cycle runs; (6) ENDWAIT ends pRDY.
- **Hold off and refresh (page 13):** half of counter U33 keeps BREQ\* asserted for a number of
  strobes after the last access (J3). The other half stops the host holding the Z80 too long:
  **the host or DMA device can run no more than 8 cycles on the internal bus before the SPUZ makes
  it wait and runs a cycle of its own**, so refresh is kept. (To defeat it: pull U24 pin 8 and
  bridge pins 8-9.)
- **The 2681 interface (page 13):** IORQ\* with WR\* or RD\* from the host or the Z80; ZA3-ZA0 pick
  the register. Its baud-rate generator and counter/timer run from the **3.6864 MHz crystal X2**.
  **The 2681 is reset whenever the Z80 is reset** (ZRST goes to both). OP7 and INTRN can both make
  ZINT\*, so the Z80 can be interrupted by the host, by both serial channels or by the
  counter/timer.
- **DRAM (page 13):** MRQ\*'s falling edge clocks U17 to start a cycle; delay line DLY1 times it
  independently of the 8 MHz clock; read data is latched in U16. Eight 64K or eight 256K parts;
  a 64K board has no U29. Factory upgrade from 64K to 256K through Viasyn.

## The DUART (pages 14-27, PDF 18-31)

Page 14 says part of Signetics' SC2681 data sheet is reprinted with permission; the full sheet is
in the Signetics MOS Microprocessor Data Manual for 1982 or later. **Pages 15-27 (PDF 19-31) are
that reprint and are not transcribed**: features, pin configuration, block diagram (page 16),
pin description, block description (channels, input/output ports, counter/timer, multidrop mode),
programming, the register bit tables, the electrical characteristics and timing diagrams. The chip model's reference is
[Signetics 2681 DUART](Signetics%202681%20DUART.md), from this reprint and Philips' 1990 and 1998 sheets. One table is kept here because the board's port
map depends on it.

**Table 1, 2681 register addressing (page 20, PDF 24).** On the SPUZ, add FC30h for the S-100 port.

| A3-A0 | Read (RDN = 0) | Write (WRN = 0) |
|---|---|---|
| 0 | Mode register A (MR1A, MR2A) | Mode register A (MR1A, MR2A) |
| 1 | Status register A (SRA) | Clock select register A (CSRA) |
| 2 | \*Reserved\* | Command register A (CRA) |
| 3 | RX holding register A (RHRA) | TX holding register A (THRA) |
| 4 | Input port change register (IPCR) | Aux. control register (ACR) |
| 5 | Interrupt status register (ISR) | Interrupt mask register (IMR) |
| 6 | Counter/timer upper (CTU) | C/T upper register (CTUR) |
| 7 | Counter/timer lower (CTL) | C/T lower register (CTLR) |
| 8 | Mode register B (MR1B, MR2B) | Mode register B (MR1B, MR2B) |
| 9 | Status register B (SRB) | Clock select register B (CSRB) |
| A | \*Reserved\* | Command register B (CRB) |
| B | RX holding register B (RHRB) | TX holding register B (THRB) |
| C | \*Reserved\* | \*Reserved\* |
| D | Input port | Output port configuration register (OPCR) |
| E | Start counter command | Set output port bits command |
| F | Stop counter command | Reset output port bits command |

From the same page: certain control registers are zero after RESET; the MR, CSR and OPCR should
be changed only while the receiver and transmitter are not enabled, and certain ACR changes only
while the C/T is stopped. MR1x and MR2x share one address through a pointer that RESET or a
"reset pointer" command points at MR1.

## Software section (pages 28-29)

Under a normal CompuPro operating system (Concurrent DOS 8-16, CP/M-86, CP/M 8-16, etc.): install
the board with the jumpers and switches set, and put **the new SPUZ SWITCH (SW!.CMD)** on the right
drive. "Eight-bit programs should now run by invoking the SPUZ through SW!.CMD." For other
environments the manual gives the 8086 program **HLT.CMD** as an example, "not ... the best way to
program the SPUZ", but showing the pitfalls. HLT.CMD puts the Z80 into a halt so that its memory is
refreshed and can be read and written:

```
START:
SELBD:   MOV  al,00h        ;00h is the board select of this board. It should be between 00h
                            ;and 0Fh.
         MOV  dx,0FC40h     ;SLAVESEL port.
         OUT  dx,al         ;Select this board.
UNRES:   MOV  al,08h        ;Bit pattern to allow Z80 to run.
         MOV  dx,0FC41h     ;PTLATCH port.
         OUT  dx,al         ;Allow Z80 to execute starting at location 0000h.
PUTHLT:  MOV  bx,0E000h     ;Memory address of SPUZ 64K segment.
         MOV  es,bx
         MOV  al,76h        ;Z80 HLT opcode is 76h.
         ES:MOV [0h],al     ;Puts 76h at E0000h, 0h in SPUZ memory.
RESET:   MOV  al,0h         ;Bit pattern to reset Z80.
         MOV  dx,0FC41h     ;PTLATCH port.
         OUT  dx,al         ;RESET Z80.
WAIT:    PUSH ax            ;Kill some time for Z80 to reset.
         POP  ax
RUN:     MOV  al,08h        ;Bit pattern to allow Z80 to run.
         MOV  dx,0FC41h     ;PTLATCH port.
         OUT  dx,al         ;Allow Z80 to execute starting at location 0000h. Its first opcode
                            ;is a HLT.
CHECK:   ES:MOV al,[0h]     ;read back in 76h (halt opcode).
         CMP  al,76h        ;make sure it is.
         JNZ  START         ;retry if it is not.
EXIT:    BACK to OS.
```

Why the steps (page 29):

- At power-up the Z80 is in reset: it does not refresh the DRAM and cannot give BUSACK. First
  select the board (one board: select 00h).
- With the board selected and the Z80 still in reset, an access to SPUZ memory "or ports FC42h to
  FC47h" **[sic: the port map on page 9 gives FC30h-FC3Fh]** hangs the S-100 bus forever, waiting
  for BUSACK. **Let the Z80 run before you access its ports or memory.**
- Writing 08h to PTLATCH releases reset (UNRES); the Z80 now runs arbitrary code from 0000h.
  Then put a HLT (76h) at 0000h (PUTHLT), reset the Z80 (RESET), wait (WAIT) and let it run again
  (RUN): it fetches the 76h and halts. Check that the 76h is still there: the Z80 could have
  overwritten it while it ran arbitrary code.
- Once the Z80 is halted it refreshes the DRAM, and the host can load programs into SPUZ memory and
  access ports FC30h-FC3Fh. To start a program: put a JUMP at 0h, then assert and release reset.

## Hardware description (pages 30-37)

### Logic diagram (pages 30-34, PDF 34-38), "SPUZ 257E, Viasyn Corporation (c) 1985"

- **Sheet 1 (page 30):** the clock (X1 16.00 MHz, U11, U12 74LS74, Q1 MPS3646), SYNCACK\*; the
  **Z80H (U10)**: A0-A15 to ZA0-ZA15 (A7 through UZA7 and the refresh toggle U32/U34/U24 to ZA7),
  D0-D7 to ZD0-ZD7 (SR5 pull-ups), RFSH\*, MRQ\*, RD\*, WR\*, IORQ\*, M1\*, HALT\* out (SR1 pull-ups);
  **BUSREQ from BREQ\*, NMI (pin 17) tied high (not used), RESET from ZRST\*, INT from ZINT\***, WAIT
  from RAS\*/M1\* through U24 and S1-4; BUSACK through U23 to BUSACK\*.
- **Sheet 4 (page 33):** the **2681 (U30)**: D0-D7 on ZD0-ZD7; CEN\* = IORQ\*, WRN\* = WR\*,
  RDN\* = RD\*; A0-A3 = ZA0-ZA3; RESET = ZRST; X1/X2 = the 3.6864 MHz crystal X2. **INTRN (pin 21)
  and OP7 (pin 15) join at ZINT\*.** **OP6 (pin 26) drives the base of Q3 (MPS3646, NPN, emitter to
  ground); its collector goes to J2 and through it to the chosen VI0\*-VI7\*.** So OP6 high (OPR
  bit 6 = 0, as after reset) pulls the VI line low: this is the "(interrupt asserted to S-100 bus)"
  of the page 12 table. OP5/OP4 = ZB1/ZB0, OP3/OP2 = DB1/DB0, OP1/OP0 = RTSB/RTSA through 1488s
  (U13) to CONN 1 pins 5; RXDA/RXDB from 1489s (U20) at CONN 1 pins 2, TXDA/TXDB through 1488s to
  pins 3; IP1/IP0 = CTSB/CTSA from CONN 1 pins 20; IP6 = HALT\*, IP5/IP4 = SB1/SB0, IP3/IP2 NC.
  (Interpretation: the two channel groups on the drawing, A and B, are two connector halves; the
  drawing labels both pin sets 2, 3, 5, 20.)
- Sheets 2, 3 and 5 (PDF 35, 36, 38): not read. Their OCR text names U27 and SLAVESEL (sheet 2)
  and pHLDA (sheet 5); sheet 3 has no readable labels in the OCR.

### Addendum: ECO 138 (PDF 39, "attached to page 35")

"SPUZ 257D and 257E addendum", an addendum to "SPUZ Technical Manual No. 11990" for revisions 257D
and E, part of **ECO 138**: to remove a possible hardware conflict, **remove pin 15 of U30 (the 2681,
OP7) from the socket** on every 257D and 257E board; corrected on **257E1 and later**. "This change
does not affect any CompuPro supplied software (SW!.CMD or SPZ.CMD)." The addendum's drawing (checked
against the image) shows **OP7 as NC and INTRN alone at ZINT\***; it draws Q3 as a 2N2222 (sheet 4 and
the parts list: MPS3646; both NPN). Document 8711-0006, copyright 1986, code 126200/1.

### Parts list (page 36, PDF 40)

BD 257C [sic: the rest of the manual is 257E]. Key parts: **Z80H 8 MHz (U10, CPU)**, **2681 (U30)**,
P258 = U22 (16L8 PAL), P372 = U25 (14L4 PAL), 25LS2521 (U31), 21212 delay line (DLY1), 1488 (U13),
1489 (U20), 74LS393 (U32, U33), 74LS175 (U21, U28), 74LS244 (U26, U37, U39, U40), 74LS266 (U27),
74LS373 (U16), 74AS258 (U9, U18), 74LS157 (U19). **64K: eight 4164 (U1-U8). 256K: eight 41256 (U1-U8)
and a 74F153 (U29).** Regulators 7805 x3, 78L12, 79L12; MPS3646 x4 (Q1-Q4); crystals 16 MHz (X1)
and 3.6864 MHz (X2); tandem shunt J2-VI0; 34-pin shrouded connector CONN 1; two 8-position DIP
switches (S1, S2).

### Component layout (page 37, PDF 41) and back matter

Page 37 is the component layout drawing (not transcribed). PDF 42: Viasyn limited warranty (one
year, six months for drives; RMA through Viasyn, 26538 Danti Court, Hayward CA 94545-3999,
(415) 786-0909; effective 9/1/85). PDF 43: back cover, 8261-0030.

## What this means for the pascal100 SPUZ model (interpretation)

The design is DESIGN.md §3.0.4.

- **Not a bus master.** The Z80 runs on the board's own internal bus. On the S-100 bus the SPUZ is
  a memory window (S2, CompuPro standard 0E0000h) and 18 ports (FC30h-FC41h, so the board decodes
  A8-A15 of the I/O address), with up to 16 boards sharing both through SLAVESEL. Host access
  borrows the Z80's bus (BUSREQ/BUSACK, J1 wait states, J3 hold off, at most 8 host cycles
  between Z80 cycles).
- **Reset state.** The Z80 and the 2681 are in reset from power-up until PTLATCH D3 = 1. While
  reset, host access to the window or FC30h-FC3Fh hangs the real bus. OPR = 0 at reset, so **VI
  (J2, VI0 standard) is pulled while the SPUZ is reset** until software sets OPR bit 6; the Z80
  starts in bank 3 on a 256K board.
- **Interrupts.** SPUZ to host: OP6 through Q3 to one VI line. Host to SPUZ: the 2681's INTRN
  (the host can reach the 2681's IMR and counter); OP7 is disconnected on current boards (ECO 138).
  NMI is not used.
- **Banks.** PB0/PB1 (PTLATCH) for the host's view, ZB0/ZB1 (OP4/OP5) for the Z80's, DB0/DB1
  (OP2/OP3) for DMA (VDMA\*, one board at a time). With 64K all of them are ignored.
- **What CompuPro's software uses** (disassembly of `SPZ.CMD` and `SW!SPUZ.CMD`, 2026-11-20): host
  SLAVESEL and PTLATCH; the 2681's ACR (FC34h, B0h), IMR (FC35h, 08h), CTUR/CTLR (FC36h/FC37h,
  3A00h), Set Output Port Bits (FC3Eh) and a read of the input port (FC3Dh, IP5/IP4 = the host
  page). The Z80 side reads 0Eh/0Fh (start/stop counter) and writes 0Eh/0Fh (set/reset OPR bits:
  its bank and the VI line). The serial channels and DMA are not used. Software maximum: 16
  boards x 4 banks = 64 eight-bit tasks.
