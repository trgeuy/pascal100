# CompuPro CPU 86/87

**Status:** done, with no 8087. The 8086 on a 16-bit bus, the Memory Manager, the 80130's
interrupt controller and timers, the power-on jump and the I/O wait state are modeled. The
8087, the 80130's ROM and outputs (jumpers J0-J7), MWRT and ΦDSB* are not (see Limitations).

## The real hardware

The **CPU 86/87** (board 186, revision B, CompuPro, a Godbout company, 1982-84) is an IEEE 696
/ S-100 processor board with an **Intel 8086** at 8 or 10 MHz. It has a socket for an **8087**
math co-processor (5 MHz only; most boards shipped without one) and an **80130** "Operating
System Firmware" chip, which CompuPro shipped **ROM-less**: only its interrupt controller and
its three timers work. The board does **8- and 16-bit transfers** on the IEEE 696 bus and finds
the width of each one by itself.

It runs **no 8-bit software**: CompuPro sold the CPU 8085/88 for that. With it, CompuPro's
CP/M-86 and MP/M 8-16 boot from their **Disk #2** through the Disk 1's 8086 boot routines
(jumper J17 at B). Disk #1 does not boot on it.

Chips: 8086, 8087 (optional), 80130 (ROM-less), 8284A-1 clock, 8288 bus controller, a 74S287
PROM for the data buffers, a 74S288 PROM for the status lines, a 74LS175 for the Memory
Manager, and 25LS2521 comparators for the switches.

Switches: S1 (the 80130's ROM address), S2 and S4-1,2 (the 80130's ports), S3 (options), S4-3..10
(the Memory Manager's port), S5 (the power-on jump). Jumpers: J0-J7 (interrupt outputs), J8 (fitted
when there is no 8087).

## Sources

| Source | Path | Authority |
|---|---|---|
| CompuPro *CPU 86/87 Technical Manual*, document #11866, 1/84, 38 pp. | `reference/CompuPro CPU 86-87.md` | Every switch, port and command word; the theory of operation; the 9-sheet logic diagram |
| CompuPro *Disk 1 Technical Manual*, theory of operation | `reference/CompuPro Disk 1 & 1A Floppy Controllers.md` | The boot EPROM answers every memory read while it boots: how an 8086 finds it at FFFF0 |
| CP/M-86 1.1PAA Disk #2 | `examples/cpm86/cpm86-11.dsk` (with R, W and HDIR added) | What CompuPro's CP/M-86 does on this board |
| MP/M 8-16 2.1F Disk #1 + `MPMGEN B:MPMLDRFY.86` | not in the repository (`Pascal100-DSDD/` in the developer's files) | What CompuPro's systems do on this board |

The power-on jump's offset byte and the odd 80130 ports are read from the drawing or not
described; the reference marks them ⚠.

## Register reference

The card's own ports. They decode **A0-A15**: see "How it is simulated".

| Addr | OUT (write) | IN (read) |
|---|---|---|
| `mm_port` (00FD) | the Memory Manager: D7-D4 = A23-A20 from the next cycle; D3-D0 ignored | not the card's (goes to the bus) |
| `port_80130` + 0 (FFF0) | ICW1 (D4 = 1), OCW2 (D4-D3 = 00), OCW3 (D4-D3 = 01) | IRR or ISR, as the last OCW3 chose |
| + 2 (FFF2) | ICW2, ICW4 after ICW1; then OCW1, the mask | the mask |
| + 8, + A, + C (FFF8, FFFA, FFFC) | timer 0, 1, 2: the count, LSB then MSB | the count (or the latched count), LSB then MSB |
| + E (FFFE) | timer control: an initialization byte or a latch command | — |

The 80130's interrupt controller works like an 8259A in 8086 mode, single, level-triggered,
specific EOI only. Its inputs IR0-IR7 are the bus lines VI0*-VI7*. The timers have **hard-wired
modes**: timer 0 mode 2 (SYSTICK), timer 1 mode 0 (DELAY), timer 2 mode 3 (BAUD).

## How it is simulated

- **The 8086** is the 8088 core (`src/cpu/cpu8088.*`, the CLK x86 core) with its 16-bit data
  bus on (`Cpu8088::setWideBus`). The instruction set is the same. A word at an even address
  is one 16-bit request (`Bus::memRead16`, sXTRQ*): a board that answers SIXTN* moves both
  bytes in one cycle. Otherwise the card runs the byte-serial pair, the **even byte first**
  (schematic 5/9, A0 = LA0 XOR FLIP), at 4 clocks more. A word at an odd address is two byte
  cycles, as on the chip. A word I/O transfer is two byte cycles, port then port + 1, because
  no I/O board in pascal100 answers SIXTN*.
- **The card's own ports decode A8-A15**, and a bus decode must use A0-A7 only (`board.h`).
  So they are not bus decodes. The card's `AddressTranslator`, which is in place only while its
  own 8086 runs, claims them (`ownsIo`). The I/O cycle still runs on the S-100 bus: observers,
  TRACE and HISTORY see it, and an 8-bit board that decodes A0-A7 answers it as on a real
  backplane. It is never reported as unclaimed. On a read the 8086 takes the 80130's byte; the
  observers see the floating bus, because the real card turns its bus buffers off (ACK*).
- **The 80130** is an `Intel8259a` and an `Intel8253` (`src/chips/`). The card forces each
  timer's mode on a control write. The timers count the **CPU clock**: the T-states of the 8086,
  which is exact both flat out and paced.
- **The Memory Manager** drives A20-A23 of every memory cycle from the next cycle on
  (`Cpu8088::setHighAddress`).
- **Interrupts:** INT* (pin 73) and NMI* (pin 12) reach the 8086. It runs two acknowledge
  cycles and takes the vector from the second; the System Support 1 answers them.
- **No timers, no events, no DMA of its own.** The run loop steps the 8086.
- **Properties:**

| Property | Default | Meaning |
|---|---|---|
| `mm_port` | `00FD` | S4-3..10 = A15-A8; the low byte is wired to FD |
| `port_80130` | `FFF0` | S2 + S4-1,2 = A15-A4, a 16-port boundary |
| `io_80130` | `true` | S3-2: the 80130's ports answer |
| `io_wait` | `true` | S3-3 + S3-4: one wait state on every I/O cycle |
| `power_on_jump` | `false` | S5-10: start at `poj_addr` instead of FFFF0 |
| `jump_on_reset` | `false` | S5-9: the power-on jump at every RESET too |
| `poj_addr` | `FF000` | S5-1..8 = A19-A12 |
| `clock_hz` | `0` | 8000000 or 10000000; 5000000 with an 8087. 0 = flat out |
| `idle` | `true` | stand down while the guest polls an empty keyboard |

### Reset

To boot: `RUN FFFF0` (MCP: `run {from: 1048560}`). `RUN <addr>` on the 8086 keeps CS when the
address is within 64K above CS*16, and otherwise sets CS = addr/16 (`Cpu8088::setPc`). After
power-on, CS = FFFF, so `RUN FFFF0` gives FFFF:0000, the real reset state. `RUN 0` runs the EPROM's 8080 code
as 8086 instructions, and it does not boot.

- `Reset::PowerOn` (POC*, cold): the 8086 is reset (CS:IP = FFFF:0000), the Memory Manager is
  cleared, and the 80130's mask is set to all masked (a model choice: see Limitations). With
  `power_on_jump`, CS:IP = `poj_addr`/16 : 0000.
- `Reset::Bus` (RESET*, warm): the 8086 is reset and the Memory Manager is cleared (CLEAR*).
  The 80130 has no reset pin and keeps its state. With `power_on_jump` and `jump_on_reset`,
  the jump again.

## Quirks reproduced

| Quirk | If you get it wrong |
|---|---|
| The byte-serial pair puts the even byte first | A board that latches on the second byte, or a trace, sees the bytes in the wrong order |
| The Memory Manager takes D7-D4, not D3-D0 | Software that writes 10H for page 1 runs in page 0, and the code after the OUT is not where it expects |
| The card's own ports decode A8-A15 | An OUT to 00FD would also latch at xxFD, and the 80130 would answer at 00F0, where the Selector Channel lives |
| The 80130's timer modes are hard-wired | The manual's own init byte for timer 0 (3AH) sets mode 5; a plain 8253 would then never tick |
| No 8087: WAIT never waits | The 8086's TEST pin is pulled low (R6); a WAIT that waited would hang |

## Limitations and deliberate departures

- **No 8087.** The socket is empty and J8 is fitted, as on most boards CompuPro shipped. The
  core runs an ESC instruction as no operation, and it runs no bus cycle for its operand. A
  real 8086 reads the operand with no 8087 present; this is one of the bus-cycle differences
  of the CLK core (`src/cpu/cpu8088.h`). Software that needs the 8087 fails, as it would on
  such a board.
- **Jumpers J0-J7 are not modelled.** The standard system fits none. The 80130's INT, its three
  timer outputs and the 8087's interrupt reach no bus line, and the 80130 never answers an
  interrupt acknowledge. A system that uses the 80130 as its interrupt controller, instead of
  the System Support 1, does not work.
- **The 80130 starts all masked.** The manual says the chip comes up "in undefined states -
  most likely ready to interrupt". With J0-J7 open this cannot be seen.
- **The 80130's ROM** (S1, S3-1) is not modelled: the part is ROM-less.
- **The odd 80130 ports, and +4 and +6,** are not described; they go to the bus.
- **The power-on jump** sets CS:IP at the reset. The real card jams three word fetches at
  FFFF0-FFFF5 onto the 8086's local bus; those bus cycles, and their clocks, are not run.
- **One wait state on every cycle** (S3-3 ON, S3-4 OFF) is not offered; `io_wait` is the
  standard I/O-only setting.
- **No prefetch queue** (the 8086's is 6 bytes). The manual warns that an OUT to the Memory
  Manager takes effect after "the next several instructions are fetched", so the bytes after it
  must be the same in both pages. Here it takes effect at the next instruction.
- **MWRT (S3-7) and ΦDSB* (S3-8)** are not modelled: the bus has no MWRT line of its own, and
  no slave processor board exists yet.
- **The System Support 1 change for MP/M** (pull U28, bend out pin 4) is not needed. U28 pin 4
  drives PHANTOM\* on the bus from the SS-1's PHANTM signal, so the change stops the SS-1 from
  asserting PHANTOM\*. The `ss1` model never asserts it (its interrupt-acknowledge PHANTOM\*
  trick has no use with this simulator's cores, `compupro-ss1.h`), so it already behaves like a
  board with pin 4 bent out. Why the real CPU 86/87 needs the change is inferred: probably the
  flip-flop sets on the 8086's acknowledge cycles and turns system memory off for reads before
  the first stack write.

## Verification

`tests/test_cpu8687.cpp`:
- the one 16-bit cycle to 16-bit RAM;
- the Memory Manager (D7-D4, A23-A20, a moved `mm_port`, and a refused one);
- the 80130's ICWs and mask, read back, with `SET BUS UNCLAIMED=HALT` in force;
- `io_80130` off;
- the hard-wired timer mode and the latch command;
- the power-on jump at power-on and at RESET (CS:IP exact);
- RESET clearing the Memory Manager, the I/O wait state, and a snapshot;
- the 8086 booting through the Disk 1's EPROM at FFFF0 (routine 0: `JMPF 0000:0006`).

`examples/cpm86` boots CP/M-86 on this board, and the examples acceptance test runs `DIR` on it.

`tests/test_cpu8088.cpp` checks the core's 16-bit bus: one cycle, byte-serial even first, odd
words, writes, and the 4-clock difference.

By hand, on 2026-11-19, with Disk 1 routine 0 or 2 (J17 at B):

| System | Result |
|---|---|
| CP/M-86 1.1PAA Disk #2, Interfacer 1 console | boots to `A>`; `DIR`, `STAT DSK:` (600K), `PIP X.CMD=STAT.CMD[V]`, the copy runs, `ERA` |
| CP/M-86 1.1PAA, `B:MPMGEN B:MPMLDRFY.86` on a copy of MP/M 8-16 2.1F Disk #1 | `COPYSYS Version 2.2P`, `Function complete.` |
| MP/M 8-16 2.1F from that disk, SS-1 console | the 8086 loader, MPM.SYS, the XIOS, the MP/M-86 2.1 banner, `0A>`; `DIR`, `STAT`, `PIP [V]`, `ERA`, `MPMSTAT`; the tick and console interrupts come through the System Support 1 |

## References

- `reference/CompuPro CPU 86-87.md` — the distilled manual.
- `reference/CompuPro Disk 1 & 1A Floppy Controllers.md` §3 — the boot EPROM and PHANTOM*.
- DESIGN.md 4.6 — contention, and the card's own I/O (`AddressTranslator::ownsIo`).
