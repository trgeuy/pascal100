# CompuPro SPUZ slave processor

**Status:** partial. The board, its window, its ports, its reset, its banks, its VI line and its
2681 are done (stage 3 of DESIGN.md §3.0.4). The debugger reaches the board Z80 as `spuz0:z80`
(stage 4). Concurrent DOS 8-16 3.1D and 4.1 run 8-bit programs on it with `SPZ.CMD` and `SW!SPUZ`
(stage 5).

## The real hardware

The SPUZ (Viasyn, board 257E, 1985) is a slave processor for the CompuPro 816 and 286 systems.
It has an 8 MHz Z80H, 64K or 256K of dynamic RAM, and a Signetics 2681 DUART with two RS-232
channels. It has no EPROM. Under Concurrent DOS 8-16, each SPUZ runs one 8-bit task in each
64K bank, so one user gets a processor, memory and serial ports of their own.

The board is not a bus master (manual p. 8). The Z80 runs on the board's own bus. The host
reaches the board through a 64K memory window and 18 ports. A host access asks the Z80 for its
bus (BUSREQ*) and waits for BUSACK*. Up to 16 boards share one window and one set of ports.

Switches and jumpers (manual pp. 4-7):

| Part | What it sets | Model property |
|---|---|---|
| SW1 5-8 | The board select, 0-15 | `select` |
| SW1 4 | One wait state in each Z80 M1 cycle | `m1_wait` |
| SW2 1-8 | A23-A16 of the window (CompuPro standard 0Eh) | `window` |
| J1 | 1, 2 or 3 S-100 wait states (B = 2, as shipped) | `waits` |
| J2 | The VI line that the board pulls (VI0, as shipped) | `interrupt` |
| J3 | The bus-acknowledge hold off, 4 or 8 cycles | not modeled |
| U1-U8 | 4164s (64K) or 41256s (256K) | `memory` |
| (none) | The debugger's record of the board Z80's instructions | `history` |

## Sources

| Source | Path | Authority |
|---|---|---|
| Viasyn *SPUZ Technical Manual*, 257E, July 1985, with the ECO 138 addendum | `reference/CompuPro SPUZ.md` | Primary for the board |
| The logic diagram, sheets 2 and 3 (PDF pp. 35-36) | read from the page images, 2026-11-20 | Primary for the decode, VPROC, PTLATCH, CLR* and PHANTOM* |
| Signetics and Philips 2681 data sheets | `reference/Signetics 2681 DUART.md` | Primary for the 2681 (`src/chips/scn2681`) |
| `SPZ.CMD` and `SW!SPUZ.CMD`, Concurrent DOS 8-16 3.1 DM | a disassembly (not in the repository) | What CompuPro's software uses |

The manual's p. 29 gives the 2681 ports as "FC42h to FC47h" [sic]. The port map on p. 9 and the
PAL decode on sheet 2 give FC30h-FC3Fh. The model uses FC30h-FC3Fh.

## Register reference

### The host side (S-100 ports, decoded on A0-A15)

| Port | OUT (write) | IN (read) |
|---|---|---|
| FC30h-FC3Fh | 2681 registers 0-F (the selected board) | 2681 registers 0-F (the selected board) |
| FC40h | SLAVESEL: every board compares D3-D0 with its select | not decoded |
| FC41h | PTLATCH (the selected board): D3 ZRST\*, D2 VDMA\*, D1-D0 the host's bank | not decoded |

Sheet 2: U36 decodes A8-A15 = FCh, and PAL U25 decodes A0-A9. So the board answers FC30h and
not 0030h.

PTLATCH (U28, a 74LS175):

| Bit | Name | 1 | 0 |
|---|---|---|---|
| D3 | ZRST\* | the Z80 and the 2681 run | the Z80 and the 2681 are in reset |
| D2 | VDMA\* | this board is the DMA board | not the DMA board |
| D1-D0 | PB1-PB0 | the 64K bank that the host sees | |

The memory window is the 64K page at `window` (A23-A16). Only the selected board decodes it.

### The board side (what the board Z80 sees)

| Z80 address | What |
|---|---|
| memory 0000h-FFFFh | the bank that 2681 OP5/OP4 select (bank 3 after reset) |
| I/O, every port | 2681 register (port AND 0Fh), 16 registers repeated |

2681 pins on the SPUZ (manual p. 12, sheet 4, ECO 138):

| Pin | Use |
|---|---|
| OP7 | not connected on 257D/E (ECO 138) |
| OP6 | high pulls the J2 VI line through Q3 |
| OP5, OP4 | ZB1, ZB0: the Z80's bank |
| OP3, OP2 | DB1, DB0: the DMA bank (stored, no effect) |
| OP1, OP0 | RTSB, RTSA |
| IP6 | HALT\* from the Z80 |
| IP5, IP4 | SB1, SB0: the host's bank (PTLATCH D1-D0) |
| IP3, IP2 | not connected (read high) |
| IP1, IP0 | CTSB, CTSA |
| INTRN | the Z80's INT\* |

## How it is simulated

The board owns a private machine: a `CpuZ80`, a `Bus` and a `Clock` at 8 MHz. The private bus
has two boards. The RAM board answers every memory cycle from the Z80's bank. The I/O board
answers every I/O cycle with the 2681. This is the pattern of the Jade Double D
(`jade-doubled.md`).

- **Catch-up.** Before a host cycle that the board decodes, the board runs its Z80 to the host's
  present: board T = host T x 8 MHz / host `hz()`, with the remainder carried. A SET of the
  host's `clock_hz` splits the interval at the change.
- **The 1 ms deadline.** While the Z80 is out of reset, a host deadline catches the board up
  once each 1 ms of host time. A change of the VI line then reaches the host with no host access.
  A halted Z80 with no 2681 event queues no deadline, so a host HLT can still be final.
- **The 2681** times itself on the board clock with its own 3.6864 MHz crystal. The board polls
  it at its `nextEvent()`. After each access, the board follows its pins: INTRN to the Z80's
  INT\*, OP6 to the VI line, OP5/OP4 to the Z80's bank.
- **Halted.** The Z80 steps once, then the board clock jumps to the 2681's next event. Only the
  2681 can end a HALT, because NMI\* is tied high (sheet 1).
- **Interrupt mode 2.** Nothing drives the private bus in the acknowledge cycle, so the vector
  byte floats to FFh. With I = FBh, the vector is at FBFFh.
- **Shared ports.** The board decodes A8-A15 on its ports. It says so with
  `Board::decodesIoHigh()`, and the bus takes the exact path for ports 30h-41h on every cycle.
  A write to FC40h is shared by every SPUZ (`sharesPort`), so it is not contention.
- **The window** decodes A16-A23 (`decodesExtended`). A SLAVESEL write that changes the
  selection calls `decodeChanged()`. A write that does not change it costs nothing.
- **PHANTOM\*.** Q4 pulls PHANTOM\* from VMA\*, the window compare (sheet 3). The board asserts
  it on every memory cycle in its window, so RAM under the window that honors PHANTOM\* steps
  aside.
- **Wait states.** A host access to the window or the 2681 holds READY for `waits` T-states.
  SLAVESEL and PTLATCH are not on the internal bus (p. 9) and add no wait. With `m1_wait`, each
  Z80 opcode fetch adds one board T-state.
- **The bus hang.** A host access to the window or to FC30h-FC3Fh while the Z80 is in reset
  hangs a real S-100 bus (pp. 9, 29). The model lets the cycle float (reads FFh, writes go
  nowhere), logs the cycle and the PC, and calls `Bus::requestBusHang()`. The run stops at the
  instruction boundary with `StopReason::BusHang` (MCP: `bus-hang`).
- **Two boards with one select.** `Board::backplaneConflict()` reports it. The loader refuses the
  file, and RUN refuses to start with `StopReason::BackplaneConflict` (MCP:
  `backplane-conflict`), not as a TMA conflict: no DMA board is involved.
- **A halted Z80 with an interrupt pending.** The 1 ms host deadline also runs while the Z80 is
  halted with interrupts on and ZINT\* low, so an interrupt with no 2681 event behind it (a CTS
  change, a host write to the IMR) still wakes it with no host access.
- **Units.** `z80` is the board Z80 (a `cpu` unit). `a` and `b` are the 2681's channels A and
  B, each a `ByteStream` (`CONNECT spuz0:a`).

### The debugger (stage 4)

The board publishes its Z80 through `Board::privateCpus()`, as a `PrivateCpu`
(`src/core/privatecpu.h`). `Machine::coreByRef("spuz0:z80")` finds it, so every command that
takes a core takes it.

- **REGS, DISASM, DUMP** read the Z80 through `PrivateCpu::peekCode`: its own bus, in the bank
  that OP5/OP4 select. No cycle runs. A bare DISASM or DUMP goes on in the core the last one
  named. The monitor keeps the core's name, not a pointer, so BOARDS REMOVE cannot leave it
  pointing at freed memory; a removed core is the bus again. SAVE FORMAT=PRN always lists
  the S-100 bus.
- **STEP spuz0:z80** runs one Z80 instruction and no host instruction. The board clock then is
  ahead of the host, and `runTo` does nothing until the host passes it. In reset, STEP is
  refused.
- **BREAK spuz0:z80 <addr>.** At the start of each run, `Debugger::run` hands the board the PC
  breakpoints bound to its Z80. `runTo` asks `shouldStop(pc)` before each step. A hit holds the
  Z80 and its clock. The run loop takes the hit at the next host boundary, so the host is at
  most one instruction past it, and `RunResult::bpCore` names the core. The next run lets the
  Z80 go, and the first step from the breakpoint is not checked again. A hit that a run did not
  take (it stopped first for another reason) is reported by the next run, unless BREAK CLEAR or
  a disable removed its breakpoint in between: then it is dropped. A held Z80 keeps the 1 ms
  deadline, so the next run catches it up. A breakpoint on the board Z80 takes no IF and no
  TRACE: the run loop judges those against its own registers at a host boundary.
- **HISTORY CPU spuz0:z80** reads the board's own ring (8192 records, as the host's). It records
  only with `history = on`. The Z80 never halts under CompuPro's scheduler, so an always-on ring
  costs time on every instruction of every board.

### Reset

CLR\* is SLVCLR\* AND pRESET\* (U34, sheet 3). It clears U28 (PTLATCH) and U17 (VPROC).

- `Reset::PowerOn`: the RAM is cleared to 0. The board is not selected. PTLATCH = 0, so the Z80
  and the 2681 are in reset. The 2681's OPR is 0, so OP6 is high and the VI line is pulled.
- `Reset::Bus`: the same, but the RAM keeps its contents.

When PTLATCH D3 goes from 1 to 0, the Z80 and the 2681 are reset. When D3 goes to 1, the Z80
starts at 0000h in bank 3.

## Quirks reproduced

| Quirk | If you get it wrong |
|---|---|
| The VI line is pulled from power-on until software sets OPR bit 6 | Concurrent DOS gets an interrupt it does not expect, or `SPZ.CMD` waits for a release that is already there |
| Only D3-D0 of SLAVESEL are compared | software that writes F0h to select board 0 selects nothing |
| The 2681 answers every Z80 I/O port, 16 registers repeated | a program that uses port 1Eh for register E fails |
| The Z80 starts in bank 3 | a program loaded into bank 0 never runs |
| A 2681 read of 2, A, C, E or F drives no data line | software that tests the byte it reads after a start counter command sees a wrong value |
| Ports decode A8-A15 | an 8-bit `OUT 40h` selects a board, or a board at port 30h collides with the SPUZ |

## Limitations and deliberate departures

- **The bus hang does not hang.** A real bus stops forever. The model floats the cycle and stops
  the run. Software that recovers from a hang (there is none) behaves differently.
- **The VI line is late by up to 1 ms of host time.** Concurrent DOS does not time the request,
  so the guest cannot see the delay.
- **Host accesses take no time from the board Z80.** The J3 hold off and the rule of 8 host
  cycles before a refresh cycle (p. 13) are not modeled. The manual gives no cycle count to
  charge.
- **VDMA and the DMA banks** (PTLATCH D2, OP3/OP2) are stored and have no effect. CompuPro's
  software does not use them. A second board that asserts VDMA\* is refused with a log line,
  because the manual warns of damage (p. 11).
- **FC40h and FC41h reads** are not decoded and float. The manual calls them write only. The PAL
  equations are not available.
- **A ByteStream carries bytes only.** Parity, framing errors and break do not travel on a line.
  The 2681's own loopback modes carry them (`src/chips/scn2681.h`).
- **The debugger reaches the board Z80 with PC breakpoints only.** A MEM or IO breakpoint
  watches the S-100 bus, not the private bus. A PC breakpoint on the board Z80 takes no IF and
  no TRACE.
- **The board's time skips while its Z80 is held.** The host time that passes between the hit
  and the host boundary is run when the Z80 is let go, so board time stays host time x 8 MHz /
  host `hz()`.
- **Every SPUZ costs host time, busy or idle.** CompuPro's scheduler never halts the Z80. Measured
  2026-11-20 (Concurrent DOS 4.1, CPU 286 flat out, the Z80 at 8 MHz, an Apple Silicon Mac), the
  emulated seconds per wall-clock second after the boot, with every board idle:

  | Boards | 1 | 2 | 4 | 8 | 16 |
  |---|---|---|---|---|---|
  | Emulated s per wall s | 2.90 | 2.69 | 2.29 | 1.85 | 1.35 |

- **Board time against host time follows the host divisor.** Flat out, every pascal100 CPU card
  gives the clock a 2 MHz divisor (DESIGN.md §8), so the CPU 286 counts as a 2 MHz part and the
  Z80 gets 4 T-states for each 80286 clock. At `clock_hz = 8000000` it gets 1.

## Verification

`tests/test_spuz.cpp` (`altair_tests spuz`) drives the board with real bus cycles at the full
16-bit port address, with `Bus::setVerify(true)`:

- the manual's HLT.CMD sequence: the Z80 runs, fetches the HLT at 0000h and halts, and IP6 shows
  HALT\*;
- FC40h selects and 0040h does not; only D3-D0 are compared;
- an access in reset floats and asks for a BusHang stop, and a host Z80 that does `IN A,(C)` with
  B = FCh stops with `StopReason::BusHang` at the next instruction;
- PTLATCH D1-D0 and the 64K board; OP5/OP4 change the Z80's bank between two fetches;
- OPR bit 6 releases the VI line, J2 moves it, and the 1 ms deadline carries it with no host
  access;
- the 2681 counter interrupts the Z80 in IM 2 through FBFFh, set up as `SPZ.CMD` does (ACR B0h,
  IMR 08h, the start counter command);
- three boards share the window with no contention; two boards with one select are refused;
- the window pulls PHANTOM\* over 1M of RAM; a bus reset deselects and resets the board;
- a snapshot taken mid-loop restores and finishes at the same board T-state;
- stage 5 (by hand, Concurrent DOS on the CPU 286 with an Interfacer 3; not in the test suite: the
  disks are CompuPro and DRI files): `SPZ` finds 1-16 boards; `SW!SPUZ` runs a BDOS 9 program and
  MBASIC 5.2; four tasks run at once in the four banks of one board, and a fifth gets "SW: No space
  available for 8 bit program"; 4 tasks on 2 boards run 2 per board. A Z80-only loop of 86M T-states
  takes 11.2 s alone, 22.4 s with 2 tasks on a board, 44.4 s with 4, 22.6-22.9 s with 4 on 2 boards and
  11.8-12.1 s with 4 on 4 boards;
- stage 4: `spuz0:z80` resolves in any case and tags its lines; a breakpoint on it holds the Z80
  and its clock, stops the host run, and is passed once on the next run; with no breakpoint the
  Z80 goes on; STEP runs the Z80 and not the host, and is refused in reset; `history` records
  only when on; the monitor's REGS, DISASM, DUMP, STEP, HISTORY and BREAK take the core, and RUN
  names the board's core and both PCs; STEP on a host core that is not active is refused;
  `DISASM spuz0:z80 CPU=z80` starts at the core's PC, a bare DUMP goes on in the core, SAVE PRN
  lists the bus, and BOARDS REMOVE leaves no stale core; a pending hit on a cleared breakpoint
  is dropped; an untimed 2681 interrupt wakes a halted Z80 by the deadline; two boards with one
  select stop the run as `backplane-conflict`.

`test_ieee696` checks that every other board decodes A0-A7 only, now also with A8-A15 = FCh.

## References

- DESIGN.md §3.0.4: the design and the decisions.
- `docs/boards/jade-doubled.md`: the first board with a processor on a private bus.
