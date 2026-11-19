# CompuPro CPU 86/87 — 8086 processor board with 8087 and 80130 sockets

Source: CompuPro *CPU 86/87 Technical Manual* (CompuPro, a Godbout company; board 186,
revision B; document #11866; file name CPU 86/87.MAN; © 1982; first edition September 1982,
second printing December 1983, third printing January 1984; 38 scanned pages, no text layer).

The **CompuPro CPU 86/87** is an IEEE 696 / S-100 processor board with an **Intel 8086**
(16-bit data bus), a socket for an **8087** math co-processor (optional, U19) and an **80130**
"Operating System Firmware" chip (U18), which CompuPro shipped **"ROM-less"**: only its interrupt
controller and its three interval timers work. The board does **8- and 16-bit transfers on the
IEEE 696 bus** and finds the width of each transfer by itself (sXTRQ*/SIXTN*). It has a
**memory manager** (a 4-bit port that drives A23-A20), a **power-on jump** to any 4K boundary
in the low megabyte, and a separate 2 MHz bus CLOCK. It runs at **8 or 10 MHz** (5 MHz with an
8087). It runs **no 8-bit software**: for that, CompuPro sold the CPU 8085/88.

This is a distilled emulation reference: the switches, the jumpers, every port, the memory
manager, the power-on jump, the 80130 as the manual describes it, and the parts of the theory of
operation that the software can see. The warranty pages are left out. Emulated as `cpu8687`
(`docs/boards/compupro-cpu8687.md`).

Page numbers below are the **printed** page numbers (printed = PDF page − 5). The logic diagram
is printed pages 24-32, "PAGE 1 OF 9" to "PAGE 9 OF 9"; it is cited here as "schematic p. N/9".

---

## 1. Quick reference for emulation

| Item | Value | Source |
|---|---|---|
| CPU | Intel 8086, maximum mode (8288 bus controller U36), 8284A-1 clock U40 | pp. 19-20, schematic 1/9, 2/9, 7/9 |
| Clock | 8 or 10 MHz as shipped; **5 MHz with an 8087** (crystal = 3 x CPU clock) | pp. 3, 8, 19 |
| Reset address | **FFFF0H**, unless the power-on jump is on (S5) | p. 7 |
| Bus width | 8- and 16-bit transfers, chosen per cycle by SIXTN* (IEEE 696) | pp. 3, 20 |
| Memory manager | **OUT 00FDH** (standard): D7-D4 of the byte = **A23-A20** from the next cycle on. D3-D0 ignored. Port low byte hard-wired FD; high byte = S4-3..10 | pp. 6, 9, 21, schematic 5/9 |
| 80130 I/O | 16 ports, **FFF0H-FFFFH** (standard): S2-1..10 + S4-1,2 = A15-A4; S3-2 enables | pp. 4-5, 11-18, schematic 8/9 |
| 80130 ROM | 16K at any 16K boundary (S1 = A23-A14); S3-1 enables; **off** (ROM-less part) | pp. 4-5 |
| Interrupts | 8086 INTR from **INT\* (pin 73)**, NMI from **NMI\* (pin 12)**; 80130 IR0-IR7 from **VI0\*-VI7\***; outputs to the bus only through jumpers J0-J7, **none fitted** as standard | pp. 7-8, 16, schematic 1/9, 9/9 |
| 8087 | socket U19; **J8 fitted = no 8087** (standard) | pp. 8-9, 19 |
| Power-on jump | S5-1..8 = A19-A12 of the target; S5-9 also on RESET; S5-10 enable; **off** as standard | p. 7, schematic 6/9 |
| Wait states | S3-3 enable, S3-4 I/O only: **one wait state on every I/O cycle** recommended | p. 5 |
| Bus CLOCK (pin 49) | 2 MHz from its own 4 MHz crystal, whatever the CPU clock | p. 21, schematic 7/9 |

### 1.1 The standard settings ("up and running in under 5 minutes", p. 1)

| Switch | Setting | Meaning |
|---|---|---|
| S1 | all OFF | 80130 ROM address (unused) |
| S2 | all OFF | 80130 I/O at FFF0 (with S4-1,2) |
| S3 | 1, 5, 6, 8 OFF; **2, 3, 4, 7 ON** | ROM off; **80130 I/O on**; one wait state on I/O cycles; MWRT made by this board; ΦDSB* off |
| S4 | 1, 2 OFF; **3-10 ON** | 80130 I/O at FFFx; **memory manager at 00FD** |
| S5 | all OFF | no power-on jump |
| J8 | fitted | no 8087 |
| J0-J7 | none | the 80130 drives no bus interrupt line |

## 2. Switches and jumpers (pp. 3-8)

All the address switches: **ON = 0, OFF = 1**.

### 2.1 S1: the 80130 ROM address (p. 4)

S1-1..10 = A23..A14. The 80130's 16K ROM can sit on any 16K boundary of the 16 MB IEEE 696
space. With the ROM-less part it does nothing; S3-1 must stay OFF so the window does not answer.
"To minimize power dissipation, turn all these paddles OFF."

### 2.2 S2 and S4-1,2: the 80130 I/O address (pp. 4-5)

S4-1 = A15, S4-2 = A14, S2-1..10 = A13..A4. The 80130 takes **16 ports on a 16-port boundary**
of the 64K I/O space. "The standard port address for this part is FFF0-F hex": all of S2 and
S4-1,2 OFF. S3-2 is the master enable.

### 2.3 S3: options (pp. 5-6)

| Paddle | Name | ON | OFF | Standard |
|---|---|---|---|---|
| 1 | Memory Enable | the 80130's 16K ROM space answers | it does not | OFF |
| 2 | I/O Enable | the 80130's 16 ports answer | they do not | **ON** |
| 3 | Wait State Enable | the on-board wait state generator works | no on-board wait states | **ON** |
| 4 | I/O Wait Only | with S3-3: one wait state on every I/O cycle | with S3-3: one on every cycle, memory and I/O | **ON** |
| 5, 6 | unused | | | OFF |
| 7 | MWRT Enable | this board makes MWRT (pin 68) | MWRT comes from elsewhere | **ON** |
| 8 | ΦDSB* Enable | a temporary master can stop the bus clock Φ with ΦDSB* (pin 21) | not | OFF |

S3-2: "Currently any interrupt driven CompuPro software uses the system support board to
arbitrate interrupts. Thus, this paddle should be turned ON so that the operating system will
program the 80130 into a repressed and passive state."

S3-3/4: "Turning this paddle on or off during program execution may cause a program crash." The
manual recommends one wait state on every I/O cycle, "to minimize problems with slow I/O boards".

S3-8: ΦDSB* on pin 21 is "a new S-100 bus line ... defined by CompuPro for use with our slave
processor boards or special DMA peripherals". "For now, leave this paddle OFF."

### 2.4 S4-3..10: the memory manager port (p. 6)

S4-3..10 = A15..A8 of the port address. "The lower byte of the port address is 'hard-wired' to
FD hex." The standard is **00FD**: S4-3..10 all ON. (U22 25LS2521 compares A15-A8 with the
switches; U33, a 74LS30, decodes A7-A2 = 1 and A1 = 0, with A0 = 1: schematic 5/9.)

### 2.5 S5: the power-on jump (p. 7, schematic 6/9)

| Paddle | Function |
|---|---|
| 1-8 | A19..A12 of the target address: any 4K boundary in the low 1 MB |
| 9 | Jump-On-Reset: ON = the jump also happens on every RESET; OFF = at power-on only |
| 10 | Power-On-Jump Enable: OFF = a normal 8086 reset (FFFF0H) every time |

"The power-on-jump feature allows execution to begin at any 4K address boundary in the lower 1
megabyte of address space by 'jamming' a long jump instruction into the processor." Standard:
all OFF.

**How the jam works (schematic 6/9).** While POJ* is active, the board turns off its own data
buffers (POJ* is an input of the buffer PROM U50) and two 81LS96 inverting buffers drive the
8086's AD lines directly:

- U17 drives **AD15-AD8**. All its inputs are tied to +5 V, and the 81LS96 inverts: AD15-AD8 =
  **00**.
- U31 drives **AD7-AD0**. Its inputs come from gates on A1 and A2 (U28, U29, U30, U42) and from
  S5-1..8.

So the 8086's three word fetches at FFFF0, FFFF2 and FFFF4 read `EA 00`, `00 00` and
`ss 00`, where `ss` = S5-1..8: `JMPF ss00:0000`, linear address `ss000`. Flip-flop U54a,
clocked by RD* with D = A2, ends POJ* after the read with A2 = 1. ⚠ The byte for the A1 = 1,
A2 = 0 word (the offset high byte) is read from the gate wiring as 00; the drawing is dense, but
the manual's "any 4K boundary" needs exactly that.

### 2.6 Jumpers J0-J7: interrupts (pp. 7-8, schematic 9/9)

| Jumper | Left side (bus) | Right side (source) |
|---|---|---|
| J7 | VI7* (pin 11) | 8087 interrupt output |
| J6 | VI6* (pin 10) | INT* (pin 73) |
| J5 | VI5* (pin 9) | 80130 interrupt output (INT) |
| J4 | VI4* (pin 8) | 80130 DELAY output (timer 1) |
| J3 | VI3* (pin 7) | 80130 BAUD output (timer 2) |
| J2 | VI2* (pin 6) | 80130 SYSTICK output (timer 0) |
| J1 | VI1* (pin 5) | INT* (pin 73) |
| J0 | VI0* (pin 4) | 8087 interrupt output |

- The 8087's interrupt goes to VI7* (lowest priority) with J7 or VI0* (highest) with J0. To use
  no interrupt controller, join the right sides of J7 and J6 (or J1 and J0): the 8087 then drives
  INT* directly.
- The 80130's INT goes to INT* by **joining the right sides of J5 and J6**.
- The three timer outputs go to VI4*-VI2* with plugs on J4-J2, or to any VI line by wire wrap.
- "The standard CompuPro system configuration requires no jumpers on J0 through J7."
- Each source drives the bus through a 7406 open-collector inverter (U44).

### 2.7 J8: the 8087 (pp. 8-9, 19)

J8 joins the 8086's RQ/GT0* to the bus request logic directly. With an 8087, J8 is removed and
RQ/GT* goes through the 8087 to the 8086, "so that DMA requests are handled correctly if the
8087 is in control of the bus". Fit J8 when there is no 8087.

Adding an 8087 to a board without one: unsolder C9, C10, X1 and L1; fit a **15 MHz** crystal
(5 MHz CPU), C9 = 220 pF and C10 = 120 pF silver mica, L1 = 1 µH; plug the 8087 into U19; remove
J8. "The fastest 8087 available runs at 5 MHz. This means that the 8086 must also run at
5 MHz." CompuPro "strongly suggest[s]" a factory upgrade instead.

The 8086's TEST pin (23) is pulled low through R6 and goes to the 8087's BUSY pin (schematic
1/9), so with no 8087, `WAIT` never waits.

## 3. The memory manager (pp. 9, 21, schematic 5/9)

"The memory manager on the CPU 86/87 is a simple 4 bit parallel port that allows control of the
upper four extended address lines (A23-20) on the S-100 Bus. The data appearing in the upper
four data bits of an I/O write to the memory manager's port address will be present on the
address lines at the next CPU cycle. The lower four data bits will be ignored."

- U46 (74LS175) latches **AD15-AD12** on the write. FD is an odd port, so an 8086 byte `OUT`
  puts its data on AD15-AD8: AD15-AD12 are **D7-D4 of the byte**. U46 is cleared by CLEAR*
  (reset): A23-A20 = 0 after a reset.
- U46's outputs pass through U45 (74LS373), latched by ALE, so the new value appears from the
  next bus cycle. A19-A16 go through U45 too.
- "The page running the code that bumps the memory manager will vanish as soon as the write
  occurs." The new page must hold known code first. The Disk 1 can put it there by DMA (it has
  the full 24-bit address), or the MPX-1 (its ROM has a function to move data between pages).
- "Since the 8086 is pipe-lined machine, the actual write to the memory manager port may occur
  after the next several instructions are fetched." The 8 or so bytes after the `OUT` must be
  the same in both pages.

## 4. The 80130 (pp. 10-18, schematic 9/9)

The 80130 is on the 8086's local bus (AD0-AD15, S0*-S2*, BHE*). When it is addressed it drives
ACK*, which turns off the board's bus data buffers (U50/U35: p. 20). Its interrupt section "is
very similar to the 8259A when operating in the 8086 mode"; its timer section "is basically a
'hard-wired' 8254".

### 4.1 Ports (with the standard base FFF0)

| Port | Write | Read |
|---|---|---|
| FFF0 | ICW1 (D4 = 1); OCW2 (D4-D3 = 00); OCW3 (D4-D3 = 01) | IRR or ISR, as the last OCW3 chose |
| FFF2 | ICW2, ICW4 after ICW1; then OCW1 (the mask) | the interrupt mask register |
| FFF8 | timer 0 count, LSB then MSB | timer 0 latched count, LSB then MSB |
| FFFA | timer 1 count | timer 1 latched count |
| FFFC | timer 2 count | timer 2 latched count |
| FFFE | timer control: an initialization byte or a latch command | — |

The manual names only the even ports. ⚠ What the odd ports in FFF0-FFFF do is not stated.

### 4.2 The interrupt controller (pp. 11-16)

- **ICW1** (FFF0, D4 = 1): D7-D6 unused, **D5 = 0** (no ICW6), **D4 = 1**, **D3 = 1, D2 = 0**
  (level-sensitive inputs, as the S-100 bus needs; no ICW5), **D1 = 1** (no slave controllers),
  **D0 = 1**. "The value to send is: 1BH." ICW1 resets the edge-sense circuits, **clears the
  mask** (all IR enabled), sets the status read to the IRR, and resets the INTA logic.
- **ICW2** (FFF2, next): D7-D3 = T7-T3 of the vector byte; the 80130 adds the level in T2-T0.
  The 8086 multiplies the byte by 4: "ICW2 sets the base address of your vector table. 80H gives
  a starting address of 200H."
- **ICW3, ICW5, ICW6:** "are never sent in the implementation of the 80130 on the CPU 86/87."
- **ICW4** (FFF2, after ICW2): D7-D5 = 0; **D4 = 0** fully nested (1 = special fully nested);
  D3 = 1, D2 = 1, D1 = 0, D0 = 1 "should always be" sent but are ignored (hard-wired: buffered,
  master, normal EOI, 8086 mode). "ICWs 1, 2 and 4 are the minimum."
- **OCW1** (FFF2, any time after the ICWs): the mask, bit n = IRn; 1 masks.
- **OCW2** (FFF0): D7-D5 = 011 (specific EOI; "the 80130 actually ignores these bits"), D4-D3 = 00,
  D2-D0 = the level. "The 80130 operates **only in the specific EOI mode**." Without the EOI,
  "that IR level and all those of a lower priority will never be interrupted again".
- **OCW3** (FFF0): D4 = 0, D3 = 1; D0 = 0 reads the IRR, D0 = 1 the ISR on later reads of FFF0.
  "The 80130 remembers the last register requested."
- **The mask** is read at FFF2 at any time.
- **Inputs:** IR7-IR0 = VI7*-VI0*, hard-wired (through U43, an 81LS96).
- **Output:** INT reaches the bus only through J5/J6 (§2.6).
- p. 10, step 1: "Whenever both boards are present, the unused controller(s) must be put in a
  benign state (all 8 or 16 interrupts masked) since the interrupt chips come up in undefined
  states - most likely ready to interrupt in some obscure manner." If the System Support 1 is the
  controller, the 80130 may simply be removed — but then J0-J7 must stay open, because U44 with
  open inputs drives the interrupt lines.
- p. 11: "The CP/M-86 software supplied by CompuPro for the CPU 86/87 contains special code to
  initialize the interrupt controller to a benign state (all interrupts masked)."

### 4.3 The timers (pp. 17-18)

Clocked by the CPU clock (80130 CLK pin, schematic 9/9). Each mode is hard-wired:

| Timer | Mode | Output | Data port | Init byte (to FFFE) | Latch command |
|---|---|---|---|---|---|
| 0 | 2, rate generator: low for one clock at zero, reloads | SYSTICK | FFF8 | `00111010` (3AH) | `00000000` |
| 1 | 0, interrupt on terminal count: low from init until the count ends, then high until reloaded | DELAY | FFFA | `01110000` (70H) | `01000000` |
| 2 | 3, square wave: low half the count, high the other half | BAUD | FFFC | `10110110` (B6H) | `10000000` |

- Load: the init byte to FFFE, then the count's LSB, then its MSB, to the timer's port. "It is
  not possible to write the count value in a single 16 bit operation." Counting starts after the
  MSB.
- Read: send the latch command to FFFE (counting goes on), then read LSB then MSB from the data
  port; after the MSB the latch follows the count again.

## 5. Theory of operation: what software can see (pp. 19-21)

- **Bus transfers.** sXTRQ* (pin 58) = LBHE* · A0 low, not in an INTA cycle: a 16-bit request.
  If a slave answers **SIXTN\*** (pin 60), ONECYCLE: one 16-bit transfer at full speed.
  Otherwise "the CPU 86/87 will halt the 8086 and read or write two bytes serially" (state
  machine U12-U14, signals STBINH, FORCE, FLIP). In the byte-serial pair, A0 on the bus is
  LA0 XOR FLIP (U11, schematic 5/9): **the even byte first, then the odd byte**.
- **Status.** A 74S288 PROM (U53) makes the S-100 status lines from S0*-S2*: sM1, sMEMR, sINP,
  sOUT, sINTA, sHLTA, sWO*, sXTRQ*.
- **Strobes.** pDBIN from MRDC*, IORC* and INTA*; pWR* from MWTC* and IOWC*. MWRT = pWR* · sOUT*
  inverted, only with S3-7 ON (schematic 7/9).
- **DMA.** HOLD*/pHLDA on the bus; RQ/GT* to the 8086 (U4-U6). "Fully compatible with DMA devices
  that adhere to the IEEE 696/S-100 standard protocols (like the DISK 1A, DISK 2, DISK3 etc.)"
- **ΦDSB\*** (pin 21), with S3-8: a temporary master stops this board's Φ clock "just after the
  rising edge", and gets it back after the next rising edge, "so that there are no slices on the
  clock line".
- **Power fail.** POC* is pulsed on the rising edge of PWRFAIL*, "just as if the power had come
  on for the first time".
- **RESET.** The 8284A's RESET goes to the 8086 and 8087; RESET* (pin 75) and SLAVE CLR* (pin 54)
  are driven from POC* (schematic 7/9).
- **2 MHz.** The bus CLOCK (pin 49) is 2 MHz from crystal X2 (4 MHz), "independent of the CPU
  frequency".
- **Slaves.** "A special clock switching circuit allows the use of specially designed slave
  processors to share the bus with the CPU 86/87. Use of an 8 bit slave CPU would provide
  equivalent operation to our CPU 8085/88 dual processor board" (p. 3).

## 6. Software (pp. 1-2)

- "Follow the switch settings as outlined in the software manual" for the other boards, except:
  a **Disk 1 revision F** (171F) or later, or a **Disk 1A**: "Move the shorting plug at jumper
  location J17 from the 'A' position ... to the 'B' position" for the 86-only boot (on the Disk 1,
  J17 B = routines 0-3, the 8086 routines). A revision E or older Disk 1 needs an upgrade EPROM.
- **MP/M 8-16** on the CPU 86/87:
  1. On the **System Support 1**, pull U28, bend out **pin 4**, and put it back so pin 4 touches
     nothing. The manual does not say why. U28 pin 4 is a hex inverter output that drives
     **PHANTOM\*** on the bus from the board's PHANTM signal (the project owner, from the SS-1
     schematic, 2026-11-19), so the change stops the System Support 1 from ever asserting
     PHANTOM\*. That signal comes from the SS-1's interrupt-acknowledge flip-flop (U44, *System
     Support 1* reference §4.4), made for the 8080/Z80 and cleared by the next sWO\*. ⚠ Why the
     CPU 86/87 needs it off is inferred, not stated: probably the flip-flop sets on the 8086's
     acknowledge cycles and turns system memory off for reads before the first stack write.
  2. Put the CPU 86/87 loader on the MP/M disk: `A>B:MPMGEN B:MPMLDRFY.86`, destination `B`.
  3. "If you are going to use the CPU 86/87 board, you will not be able to run any 8 bit
     software. Therefore, the file 'SW!.CMD' should be removed from your system disk."
- **Boot.** With the switches right, the A drive's light flashes about twice a second; insert the
  CP/M or MP/M **Disk #2**. "CP/M-86 or MP/M 86 should sign on. **CP/M-86 and MP/M 8-16 Disk #1
  will not boot with the CPU 86/87.**"

## 7. Hardware (pp. 22-23)

| Part | Function |
|---|---|
| U20 | 8086 |
| U19 | 8087 (optional) |
| U18 | 80130, ROM-less |
| U40 | 8284A-1 clock generator |
| U36 | 8288 bus controller |
| U50 | 74S287 (G193): data buffer control PROM |
| U53 | 74S288 (G192): status PROM |
| U46 | 74LS175: the memory manager latch |
| U7, U9, U22 | 25LS2521 comparators: the 80130 ROM (S1), the 80130 I/O (S2), the memory manager (S4) |
| U17, U31 | 81LS96: the power-on jump bytes |
| U43 | 81LS96: VI0*-VI7* into the 80130 |
| U44 | 7406: interrupt outputs to J0-J7 |
| X1 | CPU crystal, 3 x the CPU clock (24 MHz for 8 MHz) |
| X2 | 4 MHz, the 2 MHz bus CLOCK |

S1, S2, S4, S5: 10 positions; S3: 8 positions. J0-J7: 8 x 2; J8: 2 positions.
