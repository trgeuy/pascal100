# Signetics SCN2681 DUART

Sources (provenance in `docs/sources.md`):

1. [CompuPro SPUZ Manual.pdf](#), printed pages 15-27 (PDF pages 19-31). Viasyn reprints part of
   the Signetics data sheet "Dual Asynchronous Receiver/Transmitter (DUART) SCN2681 Series",
   marked **Preliminary**, with Signetics' permission (SPUZ manual p. 14). This is the data sheet
   of the SPUZ's own period. It has no DC or AC characteristics and no timing diagrams.
2. [SCN2681.pdf](#): Philips Semiconductors, **SCN2681 Product specification, 1998 Sep 04**
   (supersedes data of 1995 May 01; IC19 Data Handbook), 30 pages. The same part, complete.
   Every register table in it matches the reprint. It adds the test registers, the electrical
   and timing tables, and notes. Pages are cited as "1998 p. N".
3. [SCN2681-1990.pdf](#): Philips Components-Signetics, **SCN2681 Product Specification, November
   5, 1990** (document 853-1077, ECN 00932), 19 pages, a scan with no text layer. The same tables
   as 1998. Cited as "1990 p. N" (the printed page numbers, 65-83).
4. [SCC2681.pdf](#): Philips, SCC2681 Product data, 2004 Apr 06. **A different part** (the later
   CMOS DUART). Corroboration only: nothing here is taken from it alone.

Page numbers without a prefix are the reprint's printed pages. Converted 2026-11-20. The reprint
was read from the OCR text and checked against each page image (the image wins). The 1998 sheet was read from its text layer, and the 1990 sheet from its page images. Errors in the printed text are marked **[sic]**. Notes that
interpret the data sheets say so. Where this file says "the data sheets do not say", all three
are silent. Do not fill such a gap from another emulator (DESIGN.md §0.1).

The 2681 is on the [CompuPro SPUZ](../DESIGN.md) (U30, 40-pin package, 3.6864 MHz crystal). It is
the chip behind `src/chips/scn2681.{h,cpp}`.

## Features (p. 15)

- Two independent full-duplex asynchronous receiver/transmitter channels.
- Each receiver has four buffers: a 3-character FIFO and the receive shift register.
- 5 to 8 data bits; odd, even, no or forced parity; 1, 1.5 or 2 stop bits, programmable in 1/16
  bit steps.
- A baud rate for each receiver and each transmitter: 18 fixed rates from 50 to 38.4K, a rate
  from the counter/timer, or an external 1X or 16X clock.
- Parity, framing and overrun error detection; false start bit detection; line break detection
  and generation, also a break that starts in the middle of a character.
- Channel modes: normal, automatic echo, local loopback, remote loopback.
- A 16-bit counter/timer.
- A 7-bit input port. Change-of-state detection on four inputs.
- An 8-bit output port. Each bit can be set and reset alone. Outputs can be status and interrupt
  signals: up to six separate wire-OR interrupt outputs.
- One interrupt output (INTRN) with eight maskable conditions.
- Wake-up mode for multidrop use. An on-chip crystal oscillator. TTL levels, one +5 V supply.
- Maximum data rate: 1X clock 1 MB/s, 16X clock 125 KB/s.
- The 1998 sheet adds: a typical 100 kΩ pull-up on each input port pin.
- Packages: 40-pin and 28-pin DIP (0.6 inch), 24-pin DIP (0.4 inch). Ordering codes
  SCN2681AC1I28 / AC1I40 (ceramic) and AC1N24 / AC1N28 / AC1N40 (plastic), 0 to 70 °C.

## Pins (pp. 17-18)

Only the 40-pin package has all of OP2-OP7 and IP0-IP6. A3-A0, CEN, RDN, WRN, RESET, INTRN,
X1/CLK, RxDA/B and TxDA/B are on every package.

| Pin | Type | Function |
|---|---|---|
| D0-D7 | I/O | Data bus, 3-state. D0 is the LSB. |
| CEN | I | Chip enable, active low. High puts D0-D7 in the 3-state condition. |
| WRN | I | Write strobe, active low with CEN. The register loads on the **rising** edge. |
| RDN | I | Read strobe, active low with CEN. The read starts on the falling edge. |
| A0-A3 | I | Select the internal register or port. |
| RESET | I | **Active high.** See *Reset*. |
| INTRN | O | Interrupt request, active low, **open drain**. Low while one or more of the eight unmasked conditions is true. |
| X1/CLK, X2 | I, O | Crystal, or an external clock on X1/CLK (then X2 to ground). A clock must always be supplied: 2.0 to 4.0 MHz, nominally 3.6864 MHz (1998 AC table). |
| RxDA, RxDB | I | Serial input, LSB first. Mark = high, space = low. |
| TxDA, TxDB | O | Serial output, LSB first. Held at mark while the transmitter is disabled or idle, and in local loopback. |
| OP0 | O | General output, or RTSAN (channel A request to send, active low). |
| OP1 | O | General output, or RTSBN. |
| OP2 | O | General output, or the channel A transmitter 1X or 16X clock, or the channel A receiver 1X clock. |
| OP3 | O | General output, or the counter/timer output (open drain, active low), or the channel B transmitter or receiver 1X clock. |
| OP4 | O | General output, or RxRDYA/FFULLA (open drain, active low). |
| OP5 | O | General output, or RxRDYB/FFULLB (open drain, active low). |
| OP6 | O | General output, or TxRDYA (open drain, active low). |
| OP7 | O | General output, or TxRDYB (open drain, active low). |
| IP0 | I | General input, or CTSAN (channel A clear to send, active low). |
| IP1 | I | General input, or CTSBN. |
| IP2 | I | General input, or the counter/timer external clock. |
| IP3 | I | General input, or TxCA (channel A transmitter external clock; data is clocked on the falling edge). |
| IP4 | I | General input, or RxCA (channel A receiver external clock; data is sampled on the rising edge). |
| IP5 | I | General input, or TxCB. |
| IP6 | I | General input, or RxCB. |

Each IP pin has an internal pull-up to VCC of 1 to 4 µA (1998 pin table). An open input reads 1.

## Reset

RESET high (p. 17):

- clears SRA, SRB, IMR, ISR, OPR and OPCR;
- puts OP0-OP7 high (the complement of OPR = 0);
- stops the counter/timer;
- puts channels A and B in the inactive state, with TxDA and TxDB at mark.

The 1998 pin table adds: RESET **clears the test modes** (1998 p. 4) (see *Test registers*) and **sets the MR
pointer to MR1**. ISR = 00h after reset (p. 26). The 1990 sheet does not have
the test-mode clause. RESET must be high for at least **1.0 µs** (1990 p. 69); the 1998 sheet
gives 200 ns. No sheet
lists MR1, MR2, CSR, ACR, CTUR or CTLR as cleared. p. 20 says only that "the contents of certain
control registers are initialized to zero on RESET".

**The counter/timer after reset: the sources disagree.**

| Source | Text |
|---|---|
| RESET pin, reprint and 1998 | "stops the counter/timer" |
| Counter section, 1990 p. 79 and 1998 p. 17 (same words) | "On power up and after reset, the timer/counter runs in timer mode and can only be restarted. Because it cannot be shut off or stopped, and runs continuously in timer mode, it is recommended that at initialization, the output port (OP3) should be masked off through the OPCR[3:2] = 00 until the T/C is programmed to the desired operational state." |
| Counter section, SCC2681 2004 | "comes up stopped and in the timer mode. It will require a start counter command (a read at address 0xE) to start it." |

Two SCN2681 sheets (1990, 1998) say "runs" in the counter section, and the same two say "stops"
in the pin table. **The model's reading (interpretation): stopped.** "Runs" cannot be modelled
without facts that no sheet gives: the clock source and the count of the timer after reset (RESET
does not clear ACR, CTUR or CTLR). With IMR = 00h and OPCR = 00h after reset, the difference
cannot reach INTRN or OP3. A program can see it only as ISR[3], or the count at 6/7, before its
first start counter command. CompuPro's SPUZ code works with either reading: it sets ACR and the
count, and a counter-ready bit left over from a running timer would give one early time slice.

## Register map (Table 1, p. 20)

| A3-A0 | Read (RDN = 0) | Write (WRN = 0) |
|---|---|---|
| 0 | Mode register A (MR1A, MR2A) | Mode register A (MR1A, MR2A) |
| 1 | Status register A (SRA) | Clock select register A (CSRA) |
| 2 | \*Reserved\* (1998: BRG test) | Command register A (CRA) |
| 3 | RX holding register A (RHRA) | TX holding register A (THRA) |
| 4 | Input port change register (IPCR) | Auxiliary control register (ACR) |
| 5 | Interrupt status register (ISR) | Interrupt mask register (IMR) |
| 6 | Counter/timer upper (CTU) | C/T upper register (CTUR) |
| 7 | Counter/timer lower (CTL) | C/T lower register (CTLR) |
| 8 | Mode register B (MR1B, MR2B) | Mode register B (MR1B, MR2B) |
| 9 | Status register B (SRB) | Clock select register B (CSRB) |
| A | \*Reserved\* (1998: 1X/16X test) | Command register B (CRB) |
| B | RX holding register B (RHRB) | TX holding register B (THRB) |
| C | \*Reserved\* | \*Reserved\* |
| D | Input port | Output port configuration register (OPCR) |
| E | Start counter command | Set output port bits command |
| F | Stop counter command | Reset output port bits command |

- A **read** of E or F is the command; the data sheet does not say what the read returns.
- A **read** of 2 or A changes a test mode (see *Test registers*). A read of C, or a write of C:
  reserved in every SCN2681 sheet. What a read of 2, A or C returns on the data bus: the data
  sheets do not say.
- CSRx, CRx, ACR, IMR, CTUR, CTLR and OPCR are write-only. ISR, IPCR and the counter are read-only.
- CTUR and CTLR "are write-only and cannot be read by the CPU" (p. 27). Address 6/7 reads the
  counter (CTU, CTL) instead.

**The MR pointer (p. 20).** MR1x and MR2x share one address. Each channel has its own pointer.
RESET, or the "reset MR pointer" command in CRx, points it at MR1x. A read **or** a write of MR1x
moves it to MR2x. It then stays at MR2x: every later access reaches MR2x until the pointer is
reset.

### Test registers (1998 pp. 10, 24)

The reprint and the 1990 Table 1 mark reads of 2 and A as reserved. The 1990 text (p. 73) and the
1998 sheet say why:

- "The reserved registers at addresses H'02' and H'0A' should never be read during normal
  operation since they are reserved for internal diagnostics."
- **Read of 2: BRG test.** "Each read on address H'2' will toggle the baud rate test mode." In
  that mode the baud rates of all receivers and transmitters change (table below).
- **Read of A: 1X/16X test.** It "changes all transmitters and receivers to the 1x mode and
  connects the output ports to some internal nodes." The sheet does not say which nodes, or
  whether a second read turns it off.
- RESET clears both test modes.

| CSR code | Normal, ACR[7] = 0 | Normal, ACR[7] = 1 | BRG test, ACR[7] = 0 | BRG test, ACR[7] = 1 |
|---|---|---|---|---|
| 0000 | 50 | 75 | 4,800 | 7,200 |
| 0001 | 110 | 110 | 880 | 880 |
| 0010 | 134.5 | 134.5 | 1,076 | 1,076 |
| 0011 | 200 | 150 | 19.2K | 14.4K |
| 0100 | 300 | 300 | 28.8K | 28.8K |
| 0101 | 600 | 600 | 57.6K | 57.6K |
| 0110 | 1,200 | 1,200 | 115.2K | 115.2K |
| 0111 | 1,050 | 2,000 | 1,050 | 2,000 |
| 1000 | 2,400 | 2,400 | 57.6K | 57.6K |
| 1001 | 4,800 | 4,800 | 4,800 | 4,800 |
| 1010 | 7,200 | 1,800 | 57.6K | 14.4K |
| 1011 | 9,600 | 9,600 | 9,600 | 9,600 |
| 1100 | 38.4K | 19.2K | 38.4K | 19.2K |
| 1101 | Timer | Timer | Timer | Timer |
| 1110 | pin, 16X | pin, 16X | pin, 16X | pin, 16X |
| 1111 | pin, 1X | pin, 1X | pin, 1X | pin, 1X |

The 1998 table writes the pin as "I/O2"; CSRA/CSRB give the pin for each field.

**Programming rules (p. 20).** Change MR, CSR and OPCR only while the channel's receiver and
transmitter are not enabled. Make certain ACR changes only while the counter/timer is stopped.
Other changes "may cause operational problems", for example a change of bits per character
while the transmitter is active.

## Register bit formats (Table 2, pp. 21-22)

### MR1A, MR1B

| Bit | Field | Values |
|---|---|---|
| 7 | Rx RTS control | 0 = no, 1 = yes |
| 6 | Rx INT select | 0 = RxRDY, 1 = FFULL |
| 5 | Error mode | 0 = char, 1 = block |
| 4-3 | Parity mode | 00 with parity, 01 force parity, 10 no parity, 11 multidrop mode |
| 2 | Parity type | 0 = even, 1 = odd |
| 1-0 | Bits per character | 00 = 5, 01 = 6, 10 = 7, 11 = 8 |

- **[7] Rx RTS control (p. 20).** RTSAN (OP0) is normally asserted by setting OPR[0] and negated
  by resetting it. With MR1A[7] = 1, RTSAN is negated when a valid start bit arrives and the
  channel A FIFO is full. OPR[0] is **not** reset: RTSAN is asserted again when a FIFO position
  is free. Wire RTSAN to the sender's CTSN for flow control.
- **[6] Rx INT select (p. 20).** RxRDY or FFULL becomes ISR[1] (channel B: ISR[5]), and OP4
  (OP5) if OPCR makes it an interrupt output.
- **[5] Error mode (pp. 20, 22).** Char: SR[7:5] is the status of the character at the top of the
  FIFO. Block: SR[7:5] is the OR of the status of every character that came to the top of the
  FIFO since the last "reset error status" command.
- **[4:3] Parity mode (p. 22).** With or force parity: the transmitter adds a parity bit, and the
  receiver checks it. 11 = multidrop mode (see *Multidrop mode*).
- **[2] Parity type (p. 22).** Odd or even for "with parity"; the value of the forced bit for
  "force parity"; no effect with no parity; the polarity of the A/D bit in multidrop mode.
- **[1:0] (p. 22).** The count excludes the start, parity and stop bits.

### MR2A, MR2B

| Bit | Field | Values |
|---|---|---|
| 7-6 | Channel mode | 00 normal, 01 auto echo, 10 local loop, 11 remote loop |
| 5 | Tx RTS control | 0 = no, 1 = yes |
| 4 | CTS enable Tx | 0 = no, 1 = yes |
| 3-0 | Stop bit length | see below |

Stop bit length (bits), for 6, 7 and 8 bits per character. "Add 0.5 to values shown for 0-7 if
channel is programmed for 5 bits/char."

| Code | Bits | Code | Bits | Code | Bits | Code | Bits |
|---|---|---|---|---|---|---|---|
| 0 | 0.563 | 4 | 0.813 | 8 | 1.563 | C | 1.813 |
| 1 | 0.625 | 5 | 0.875 | 9 | 1.625 | D | 1.875 |
| 2 | 0.688 | 6 | 0.938 | A | 1.688 | E | 1.938 |
| 3 | 0.750 | 7 | 1.000 | B | 1.750 | F | 2.000 |

- **[7:6] Channel mode (pp. 22-23).** A mode change acts at once, also in the middle of a
  character. Exception: if auto echo or remote loopback is deselected just after the receiver
  sampled the stop bit (in auto echo, RxRDY is asserted), and the transmitter is enabled, the
  transmitter stays in auto echo until the whole stop bit is retransmitted.
  - **Auto echo (01):** received data is reclocked and sent on TxD; the receive clock drives the
    transmitter; the receiver must be enabled, the transmitter need not be; TxRDY and TxEMT are
    inactive; parity is checked but sent as received; framing is checked, and the stop bits are
    sent as received; a break is echoed until the next valid start bit; the CPU still reads the
    receiver, but the CPU-to-transmitter link is disabled.
  - **Local loopback (10):** the transmitter output goes to the receiver input; the transmit clock
    drives the receiver; TxD is held high; RxD is ignored; the transmitter must be enabled, the
    receiver need not be; the CPU reads and writes normally.
  - **Remote loopback (11):** received data is "relocked" **[sic: reclocked]** and sent on TxD;
    the receive clock drives the transmitter; received data does not reach the CPU and the error
    status is inactive; parity is not checked and is sent as received; the receiver must be
    enabled; framing is not checked, and the stop bits are sent as received; a break is echoed
    until the next valid start bit.
- **[5] Tx RTS control (p. 23).** With MR2A[5] = 1, OPR[0] is reset automatically one bit time
  after the shift register and the THR are empty (stop bits included), if the transmitter is not
  enabled. Use: set MR2A[5]; enable the transmitter; set OPR[0] (RTSAN asserted); send; disable
  the transmitter after the last character goes into THR. RTSAN is negated one bit time after
  the last stop bit.
- **[4] CTS enable Tx (p. 23).** 0: CTSAN (IP0) has no effect. 1: the transmitter checks CTSAN
  before each character. Low sends it; high holds TxD at mark until CTSAN goes low. A change
  during a character does not affect that character.
- **[3:0] Stop bit length (p. 23).** 9/16 to 1 and 1-9/16 to 2 bits, in 1/16 steps, for 6, 7 and
  8 bits per character; 1-1/16 to 2 bits for 5. The receiver checks only for mark at the center
  of the first stop bit, in every case. With an external 1X transmit clock, MR2A[3] = 0 sends one
  stop bit and 1 sends two.

MR1B and MR2B are the same for channel B (p. 23). The channel B pointer moves the same way, through
CRB.

### CSRA, CSRB (p. 23)

| Bits | Field |
|---|---|
| 7-4 | Receiver clock select |
| 3-0 | Transmitter clock select |

Baud rate, with a 3.6864 MHz clock. ACR[7] picks the set.

| Code | ACR[7] = 0 | ACR[7] = 1 |
|---|---|---|
| 0000 | 50 | 75 |
| 0001 | 110 | 110 |
| 0010 | 134.5 | 134.5 |
| 0011 | 200 | 150 |
| 0100 | 300 | 300 |
| 0101 | 600 | 600 |
| 0110 | 1,200 | 1,200 |
| 0111 | 1,050 | 2,000 |
| 1000 | 2,400 | 2,400 |
| 1001 | 4,800 | 4,800 |
| 1010 | 7,200 | 1,800 |
| 1011 | 9,600 | 9,600 |
| 1100 | 38.4K | 19.2K |
| 1101 | Timer | Timer |
| 1110 | external pin, 16X | external pin, 16X |
| 1111 | external pin, 1X | external pin, 1X |

The external pin for codes 1110 and 1111:

| Field | Pin |
|---|---|
| CSRA[7:4], channel A receiver | IP4 |
| CSRA[3:0], channel A transmitter | IP3 |
| CSRB[7:4], channel B receiver | IP6 |
| CSRB[3:0], channel B transmitter | IP5 |

The clock is always 16X, except for code 1111.

### CRA, CRB (pp. 21, 24)

| Bit | Function |
|---|---|
| 7 | Not used, must be 0 (1998: "should be set to zero for upward compatibility with newer parts") |
| 6-4 | Miscellaneous command |
| 3 | Disable Tx (1 = yes) |
| 2 | Enable Tx (1 = yes) |
| 1 | Disable Rx (1 = yes) |
| 0 | Enable Rx (1 = yes) |

One write can give several commands if they do not conflict. **Consecutive writes to the same
command register need at least three edges of the X1 clock between them** (1998 p. 6, AC note 8; Table
2 note: "Access to the upper four bits of the command register should be separated by three (3)
edges of the X1 clock"). "Enable transmitter" and "reset
transmitter" cannot be in one command word.

| CR[6:4] | Command |
|---|---|
| 000 | No command. |
| 001 | Reset MR pointer: the pointer points to MR1. |
| 010 | Reset receiver: as a hardware reset of the receiver. The receiver is disabled and the FIFO is flushed. |
| 011 | Reset transmitter: as a hardware reset of the transmitter. |
| 100 | Reset error status: clears received break, parity error, framing error and overrun error (SR[7:4]). In char mode it clears OE (RB, PE and FE are also cleared); in block mode it clears all error status after a block. |
| 101 | Reset break change interrupt: clears the channel's change-in-break bit (ISR[2] for A, ISR[6] for B). |
| 110 | Start break: TxD goes low (space). Transmitter empty: the break starts within two bit times. Transmitter active: it starts when the character is done. Character in THR: it starts after that character and any loaded after it. The transmitter must be enabled. |
| 111 | Stop break: TxD goes high (mark) within two bit times. TxD stays high for one bit time before the next character, if any. |

- **[3] Disable Tx.** Stops the transmitter and resets TxRDY and TxEMT. A character being sent, or
  one in THR, is sent first; then the transmitter is inactive.
- **[2] Enable Tx.** TxRDY is asserted.
- **[1] Disable Rx.** Stops the receiver at once; a character being received is lost. No effect on
  the receiver status bits or other control registers. In multidrop mode the receiver operates
  even when it is disabled.
- **[0] Enable Rx.** Outside wake-up (multidrop) mode, it also puts the receiver in the
  search-for-start-bit state.

### SRA, SRB (pp. 21, 24-25)

| Bit | Field |
|---|---|
| 7 | Received break \* |
| 6 | Framing error \* |
| 5 | Parity error \* |
| 4 | Overrun error |
| 3 | TxEMT |
| 2 | TxRDY |
| 1 | FFULL |
| 0 | RxRDY |

\* "These status bits are appended to the corresponding data character in the receive FIFO. A
read of the status register provides these bits (7:5) from the top of the FIFO together with
bits 4:0. These bits are cleared by a 'reset error status' command. In character mode they are
discarded when the corresponding data character is read from the FIFO."

- **[7] Received break.** An all-zero character of the programmed length arrived with no stop bit.
  A break takes one FIFO position. Further entries wait until RxD is at mark for at least half a
  bit time (two edges of the internal or external 1X clock). The change-in-break ISR bit is set
  at the start and at the end of a break. A break that starts in the middle of a character must
  last at least to the end of the next character time to be detected.
- **[6] Framing error.** No stop bit was detected for the character. The check is at the middle of
  the first stop bit position.
- **[5] Parity error.** With or force parity, the character had wrong parity. In multidrop mode,
  this bit holds the received A/D bit.
- **[4] Overrun error.** A new character arrived while the FIFO was full and a character was
  already waiting in the shift register. The waiting character, and its break, parity and
  framing status, is lost. The bit is set at the start bit of the new character (p. 19). Cleared
  by "reset error status".
- **[3] TxEMT.** The THR and the shift register are both empty: set after the last stop bit if no
  character is in THR. Reset when the CPU loads THR or disables the transmitter.
- **[2] TxRDY.** THR is empty. Cleared when the CPU loads THR; set when the character moves to the
  shift register. Reset when the transmitter is disabled and set when it is first enabled. A
  character written to THR while the transmitter is disabled is not sent.
- **[1] FFULL.** Set when a transfer from the shift register fills all three FIFO positions. Reset
  when the CPU reads RHR. If a character waits in the shift register, FFULL stays set after the
  read.
- **[0] RxRDY.** A character is in the FIFO. Set when one arrives; reset by a read of RHR if no
  character is left.

### OPCR (pp. 21, 25)

| Bit | Output | 0 | 1 / other codes |
|---|---|---|---|
| 7 | OP7 | complement of OPR[7] | TxRDYB |
| 6 | OP6 | complement of OPR[6] | TxRDYA |
| 5 | OP5 | complement of OPR[5] | RxRDY/FFULLB |
| 4 | OP4 | complement of OPR[4] | RxRDY/FFULLA |
| 3-2 | OP3 | 00 complement of OPR[3] | 01 C/T output, 10 TxCB (1X), 11 RxCB (1X) |
| 1-0 | OP2 | 00 complement of OPR[2] | 01 TxCA (16X), 10 TxCA (1X), 11 RxCA (1X) |

- The 1990 pin table (p. 67) is misaligned: from TxDB down, each row carries the text of the
  pin below it (TxDB has OP0's text, OP0 has OP1's). Its Table 2 and text are correct.
- The 1998 text calls OP5 "the Channel B transmitter interrupt output which is the complement of
  ISR[5]" (1998 p. 16) **[sic: receiver]**. Its own Table 2 and the reprint say RxRDY/FFULLB.
- OP7 and OP6 as interrupt outputs are the complement of TxRDYB and TxRDYA. OP5 and OP4 are the
  complement of ISR[5] and ISR[1]. In these modes the output is open collector, and the **IMR does
  not mask it**.
- OP3 as the C/T output is open collector and not masked by the IMR. Timer mode: a square wave at
  the programmed frequency. Counter mode: high until terminal count, then low; high again when a
  stop counter command stops the counter.
- A 1X clock output runs free when no data is being sent or received. OP2's TxCA (16X) is the
  clock selected by CSRA[3:0], and a 1X clock if CSRA[3:0] = 1111.

### ACR (pp. 21, 25-26)

| Bit | Field |
|---|---|
| 7 | BRG set select: 0 = set 1, 1 = set 2 |
| 6-4 | Counter/timer mode and clock source (Table 4) |
| 3-0 | Delta IP3, IP2, IP1, IP0 interrupt enable: 1 = on |

Set 1: 50, 110, 134.5, 200, 300, 600, 1.05K, 1.2K, 2.4K, 4.8K, 7.2K, 9.6K, 38.4K. Set 2: 75, 110,
134.5, 150, 300, 600, 1.2K, 1.8K, 2.0K, 2.4K, 4.8K, 9.6K, 19.2K.

**Table 4, ACR[6:4] (p. 26):**

| ACR[6:4] | Mode | Clock source |
|---|---|---|
| 000 | Counter | External (IP2) |
| 001 | Counter | TxCA: 1X clock of the channel A transmitter |
| 010 | Counter | TxCB: 1X clock of the channel B transmitter |
| 011 | Counter | Crystal or external clock (X1/CLK) divided by 16 |
| 100 | Timer | External (IP2) |
| 101 | Timer | External (IP2) divided by 16 |
| 110 | Timer | Crystal or external clock (X1/CLK) |
| 111 | Timer | Crystal or external clock (X1/CLK) divided by 16 |

ACR[3:0]: a delta bit in IPCR sets ISR[7] only if its enable bit is on. ISR[7] then interrupts if
IMR[7] = 1.

### IPCR (pp. 21, 26)

| Bits | Field |
|---|---|
| 7-4 | Delta IP3, IP2, IP1, IP0: 1 = a change of state |
| 3-0 | IP3, IP2, IP1, IP0: the pin now, 1 = high |

A high-to-low or low-to-high change that lasts longer than 25-50 µs sets the delta bit (p. 18).
The 1990 (p. 71) and 1998 sheets explain the figure: the detector samples on a 38.4 kHz clock
from a baud rate generator tap (a period of slightly more than 25 µs with a 3.6864 MHz clock). It
needs two successive samples at the new level. So the change must last 25 µs if it starts with a
sample pulse, and 50 µs if it "just missed" one. A
read of IPCR clears bits 7-4 and ISR[7]. Bits 3-0 are not latched.

### Input port (address D, read; p. 18)

Bits 6-0 are IP6-IP0, unlatched: a high input reads 1. **D7 always reads 1.**

### ISR (pp. 22, 26)

| Bit | Condition |
|---|---|
| 7 | Input port change |
| 6 | Delta break B |
| 5 | RxRDY/FFULLB |
| 4 | TxRDYB |
| 3 | Counter ready |
| 2 | Delta break A |
| 1 | RxRDY/FFULLA |
| 0 | TxRDYA |

- INTRN is asserted while (ISR AND IMR) is not zero. The IMR does not mask a read of the ISR, or
  the interrupt outputs on OP3-OP7. ISR = 00h after reset.
- **[7]** a selected IP0-IP3 change (ACR[3:0]); cleared by a read of IPCR.
- **[6], [2]** start or end of a received break; cleared by the channel's "reset break change
  interrupt" command.
- **[5], [1]** RxRDY or FFULL, as MR1x[6] selects; follows the SR bit (set again after the FIFO is
  popped while characters remain). The FFULL text says a character is "transferred from the
  receive holding register to the receive FIFO" **[sic: from the receive shift register]**.
- **[4], [0]** duplicates of TxRDYB (SRB[2]) and TxRDYA (SRA[2]).
- **[3] Counter ready.** Counter mode: set at terminal count, reset when a stop counter command
  stops the counter. Timer mode: set once each cycle of the square wave (every second time the
  counter reaches zero); reset by a stop counter command, which in timer mode **does not stop**
  the counter/timer.

### IMR (pp. 22, 26)

The same bit layout as the ISR. 1 = the condition can assert INTRN.

### CTUR, CTLR (pp. 22, 27)

CTUR holds bits 15-8 and CTLR bits 7-0 of the count. **The minimum value is 0002h.** Both are
write-only.

## Operation

### Transmitter (p. 19)

- Enable it through CRx. TxRDY in SR says THR is free; it can interrupt at OP6/OP7 and INTRN.
  Loading THR clears TxRDY. The character moves to the shift register when the shift register is
  idle, and TxRDY is set again: one character time of buffering.
- A character cannot be loaded while the transmitter is disabled.
- It sends a start bit, the data bits (LSB first), an optional parity bit and the programmed stop
  bits. With no new character in THR, TxD stays high and TxEMT is set. Loading THR clears TxEMT
  and transmission resumes.
- A disabled transmitter finishes the character it is sending.
- "Start break" holds TxD low.
- "Reset transmitter" stops it at once. It must then be enabled again.
- With CTS enabled (MR2x[4]), CTSN must be low for a character to start. If CTSN goes high during
  a character, the character is finished and TxD then stays at mark until CTSN goes low.
- See MR2x[5] for the automatic negation of RTSN.

### Receiver (p. 19)

- Enable it through CRx. It looks for a high-to-low (mark to space) transition on RxD. 16X clock:
  it samples RxD on each 16X clock for 7-1/2 clocks. 1X clock: it samples at the next rising edge
  of the bit clock. RxD high again: a false start bit, and the search starts again. RxD still low:
  a valid start bit. It then samples at the center of each bit until the data bits, the parity
  bit (if any) and one stop bit are in. LSB first.
- The character goes into RHR and RxRDY in SR is set. This can interrupt at OP4/OP5 and INTRN.
  With fewer than 8 bits per character, the unused high bits of RHR are 0.
- After the stop bit, it looks for the next start bit at once. A non-zero character with a framing
  error, with RxD still low half a bit after the stop bit was sampled: the receiver acts as if a
  start bit began at that point.
- Parity error, framing error, overrun error and received break go into SR at the character
  boundary, before RxRDY is set.
- A break (RxD low for a whole character, stop bit included) loads an all-zero character into RHR
  and sets the received break bit. RxD must be high for at least half a bit time before the search
  for the next start bit starts.
- **The FIFO.** RHR is a 3-character FIFO. A character goes into the topmost empty position.
  RxRDY: one or more characters are in it. FFULL: all three are full. Either can be chosen to
  interrupt. A read of RHR gives the top character and pops the FIFO with its status. Each
  character carries parity error, framing error and received break; overrun is not carried.
  Reading SR does not pop the FIFO, so read SR before RHR.
- **Overrun.** If the FIFO is full, a new character waits in the shift register. If one more
  arrives "while this state exits" **[sic: exists]**, the FIFO does not change, the character in
  the shift register is lost, and SR[4] is set at the start bit of the new character.
- **Receiver-controlled RTS** (MR1x[7]): RTSN is negated when a valid start bit arrives and the
  FIFO is full, and asserted again when a position is free.

### Multidrop mode (p. 20)

MR1x[4:3] = 11 selects it. A master sends an address character and then data characters; the
slaves wake when they see their address.

- The transmitter sends a start bit, the data bits, an A/D bit and the stop bits. MR1x[2] sets the
  A/D bit: 0 = data, 1 = address. Program MR1x before you load the data bits into THR.
- The receiver watches the line whether it is enabled or not. **Disabled:** it sets RxRDY and
  loads the character into the FIFO if the A/D bit is 1 (an address), and discards it if 0
  (data). **Enabled:** every character goes to the CPU. In both cases the data bits go into the
  FIFO, and the A/D bit goes into the status bit normally used for parity error (SR[5]). Framing
  error, overrun error and break detect work as normal, enabled or not.

### Output port (p. 19)

The outputs are the **complement** of the OPR: OPR[n] = 1 makes OP[n] low. A write to address E
sets the OPR bits that are 1 in the data. A write to address F resets them. A 0 in the data leaves
the bit as it is. MR1x, MR2x and OPCR can give each output a special function.

### Counter/timer (pp. 18, 27)

- **Timer mode** (programmable divider): a square wave with a period of twice the count (in clock
  periods). A new CTUR/CTLR value does not change the current half period; it changes the next
  ones. The timer runs continuously. A **start counter** command (a read with A3-A0 = 1110)
  ends the current cycle and starts a new one with CTUR/CTLR. Counter ready (ISR[3]) is set once
  each square-wave cycle. A **stop counter** command resets ISR[3] and does not stop the timer.
  The square wave goes to OP3 if OPCR selects it. The timer output can also be a baud-rate clock
  (CSR code 1101): it then acts as the 16X clock.
- **Counter mode:** a **start counter** command (a read with A3-A0 = 1110) starts a count down from
  CTUR/CTLR. At terminal count (0000h) ISR[3] is set. **The counter continues past terminal count
  until the CPU stops it.** The OP3 output, if selected, is high until terminal count, then low.
  A **stop counter** command (a read with A3-A0 = 1111) stops the counter, returns OP3 high and
  clears ISR[3].
- New CTUR/CTLR values take effect at the next start counter command. If no new values are loaded,
  the old values are used for the next cycle.
- In counter mode the CPU can read the count (CTU at 6, CTL at 7). Stop the counter first: a carry
  from the low to the high byte between the two reads gives a wrong value. A start counter command
  starts a new count cycle from CTUR/CTLR.

**What clears an interrupt** (1990 p. 69 and 1998 AC table, "INTRN (or OP3-OP7 when used as
interrupts) negated from", each within 300 ns): read RHR (RxRDY/FFULL); write THR (TxRDY); a reset
command (delta break); a stop C/T command (counter); read IPCR (input port change); write IMR
(clear of an interrupt mask bit).

The data sheets do not say:
- what the counter holds after it passes terminal count (interpretation: it continues down from
  0000h, through FFFFh);
- what CTU and CTL read in timer mode;
- what a read of E or F returns on the data bus;
- the delay between a start counter command and the first count.

### Figures

Both Philips sheets have timing diagrams: reset, bus, port, interrupt, clock, transmitter,
receiver (FIFO, overrun, RTS on OP0) and wake-up mode (1990 figures 1-10, pp. 80-83; 1998
figures 3-12). Use the receiver figure to test the FIFO: with MR1[7] = 1, D5 is held in the shift
register while the FIFO is full and is lost when D6 arrives; overrun (SR[4]) is set at D6's start
bit and stays set until the reset command.

### Receiver lock-up (1998 p. 24)

"A condition that occurs infrequently has been observed where the receiver will ignore all data.
It is caused by a corruption of the start bit generally due to noise." The receiver then seems
asleep until it is reset. Recovery: a receiver software reset, then a receiver enable. In wake-up
mode, leave wake-up mode first, reset, and enter it again. "All receiver data, status and
programming will be preserved." A simulated line has no noise, so the model does not have this
fault.

## Timing (1998 AC characteristics, the parts a model uses)

| Item | Value |
|---|---|
| X1/CLK frequency | 2.0 min, 3.6864 typical, 4.0 max MHz |
| X1/CLK high or low time | 100 ns min |
| RESET pulse width | 1.0 µs min (1990); 200 ns min (1998) |
| High time between reads and/or writes (tRWD) | 200 ns min |
| INTRN negated after a stop counter command | 300 ns max |
| CTCLK (IP2) high or low time | 100 ns min |
| Writes to one command register | 3 X1 edges apart |

The 200 ns and 3-edge rules are rules for the CPU. On the SPUZ they are far shorter than one Z80
I/O cycle at 8 MHz, so the model does not check them (interpretation).

## How CompuPro's SPUZ software uses the chip (interpretation)

From the disassembly of `SPZ.CMD` and `SW!SPUZ.CMD` (Concurrent DOS 8-16 3.1 DM), 2026-11-20:

- **ACR = B0h:** BRG set 2; counter mode, X1/CLK / 16 (230.4 kHz from 3.6864 MHz).
- **CTUR/CTLR = 3A00h:** 14,848 counts, about 64.4 ms, the Z80's time slice.
- **IMR = 08h:** counter ready only; INTRN drives the SPUZ Z80's INT.
- **Set OPR bits 4Ch** (6, 3, 2): OP6 low releases the SPUZ's VI line; OP3/OP2 low select DMA
  bank 0.
- The Z80 reads **F (stop counter)** in its tick handler to clear ISR[3], and reads **E (start
  counter)** when it resumes a task. It writes **F (reset OPR bits) 30h** and **E (set OPR bits)**
  to change its own bank (OP5/OP4), and **F 40h** to pull the VI line (OPR[6] = 0, OP6 high).
- The host reads **D (input port)**: IP5/IP4 are the host's bank on the SPUZ, IP6 is the Z80's
  HALT\*.
- The serial channels are not used by this software. The SPUZ wires RxDA/B, TxDA/B, OP1/OP0
  (RTS) and IP1/IP0 (CTS) to its RS-232 connector.
