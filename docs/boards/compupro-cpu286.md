# CompuPro CPU 286

**Status:** done, real mode only, with no 80287. The 80286 on a 16-bit bus with Intel's clock
counts, the memory manager, jumpers J1-J7 and the EPROM sockets are modeled. Protected mode,
the 80287, SRAM or EEPROM in the sockets (J8, J9) and CDSB* are not (see Limitations).

## The real hardware

The **CPU 286** (board 451B, Viasyn, 1986) is an IEEE 696 / S-100 processor board with an
**Intel 80286** at 8, 10 or 12.5 MHz. It has a socket for an **80287** and two sockets (U37,
U38) for 64K of EPROM, SRAM or EEPROM in page FF. It does **8- and 16-bit transfers** on the
IEEE 696 bus. It has **no interrupt controller**: the System Support 1 supplies one. The SS-1
needs ECO 179.

It runs **no 8-bit software**. CompuPro's CP/M-86 and MP/M 8-16 boot on it through the Disk
1's 8086 boot routines, as on the CPU 86/87. The manual's notes are for Concurrent DOS 8-16
4.1.

Jumpers: J1 (wait states), J2 (the card's ports back to the bus), J3-J6 (A23-A20 from the
memory manager or from the 80286), J7 (the sockets on), J8 and J9 (the chip type in the
sockets).

## Sources

| Source | Path | Authority |
|---|---|---|
| Viasyn *CPU 286 Technical Manual*, doc 8261-0018B, 1986, 30 pp. | not in the repository (`Documentation/CompuPro/` in the developer's files) | Every jumper and port; the theory of operation; the 5-sheet schematic |
| SingleStepTests 80286 v1_real_mode | `tests/cpu/80286/` (a sample) | The 80286's instruction set (`docs/sources.md`) |
| CP/M-86 1.1PAA Disk #2 | `examples/cpm86/cpm86-11.dsk` | What CompuPro's CP/M-86 does on this board |
| MP/M 8-16 2.1F Disk #1 + `MPMGEN B:MPMLDRFY.86` | not in the repository | What CompuPro's MP/M 8-16 does on this board |

## How it starts

The 80286 starts at F000:FFF0 with a hidden CS base of FF0000. Its first fetch is at
linear FFFFF0. Jumpers J3-J6 decide which address reaches the bus:

- **A-C (as shipped, `mm_lines = F`).** The memory manager drives A20-A23. RESET clears it,
  so the first fetch is at **0FFFF0**. The Disk 1 or Disk 1A boot ROM answers there under
  PHANTOM*. The Disk 1A's routines 4 and 5 boot from a DISK 3 hard disk.
- **B-C (`mm_lines = 0`).** The 80286 drives A20-A23. The first fetch is at **FFFFF0**, in the
  sockets when J7 is fitted. A20-A23 stay high until the first far jump loads CS.

The monitor's `RUN` with no address starts the 80286 from its reset state. `RUN FFFF0` does
not: it loads CS = FFFF, IP = 0.

## Register reference

| Port | Direction | Function |
|---|---|---|
| 01F8, 01FA, 01FC, 01FE | write | The memory manager: D0-D3 = A20-A23. RESET clears it. |
| 01F9-01FF (odd) | write | The real board latches random data. Here the latch does not change. |
| 01F8-01FF | read | Does not reach the S-100 bus. Reads FF. |
| 00F8-00FF | read/write | The 80287. None is fitted: reads give FF. |

With J2 removed (the standard), the card decodes all of A0-A15 for these 16 ports. It turns its
S-100 data buffers off for them. The bus cycle still runs. With J2 fitted, all 16 ports go to
the S-100 bus, and the memory manager cannot be written.

## Properties

| Property | Jumper | Default | Function |
|---|---|---|---|
| `mm_lines` | J3-J6 | `F` | The A20-A23 lines that the memory manager drives. Bit 0 = A20 (J6), bit 3 = A23 (J3). |
| `j2` | J2 | `false` | The card's ports go back to the S-100 bus. |
| `sockets` | J7 | `false` | Page FF (FF0000-FFFFFF) comes from the sockets, not the S-100 bus. |
| `rom` | U37/U38 | empty | The EPROM pair as one 16-bit image: HEX, S-records, binary or `builtin:<name>`. |
| `waits` | J1 | `B` | The J1 positions fitted. The wait states add together. |
| `clock_hz` | | `0` | The CPU clock. 0 runs flat out, and boards time it as 8 MHz. |
| `idle` | | `true` | Stand down when the guest only polls an empty keyboard. |

The `rom` image starts at FF0000. The board rounds it up to 16K (two 2764s), 32K (two 27128s)
or 64K (two 27256s) and repeats it through page FF. A write to the sockets is lost. The sockets
are on the card's side of its buffers: their cycles do not run on the S-100 bus. Not checked
on the schematic: whether the address and status of a socket cycle appear on the bus.

J1 positions: A = 1 memory wait, B = 1 I/O, C = 1 on all cycles, D = 2 memory, E = 2 I/O,
F = 2 on all cycles, G = 4 on the sockets, H = 1 on a memory write and 1 on I/O. The manual
does not give a factory setting. The default B is its minimum: one wait state on every I/O
cycle.

## Limitations

- **Real mode only.** An LMSW that sets PE is not supported.
- **No 80287.** ESC instructions do nothing. Its ports read FF.
- **Timing.** The core uses Intel's clock counts, checked against a real chip
  (`docs/sources.md`). It does not model the prefetch queue: Intel says real code runs 5-10%
  over its counts. Code is fetched a word per bus cycle, from an even address, and an
  instruction that starts in the word the previous one ended in does not fetch that word
  again. Every wait state from J1 adds a clock, also on writes and fetches, where the real
  chip can sometimes hide one.
- **Shutdown is shown as a halt.** When the 80286 cannot push FLAGS, CS and IP for an
  interrupt or exception (SP = 1, 3 or 5), it shuts down, as Intel describes: it stops, INTR
  cannot wake it, NMI or RESET can. The run stops with `halt`. The HALT bus cycle with A1 low
  is not modeled.
- **Only EPROMs in the sockets.** J8 and J9 (SRAM, EEPROM) are not modeled.
- **CDSB*** (pin 19) is not modeled.
- **The disassembler** knows the 8086 instructions only. It does not show the 80286 additions
  (PUSHA, ENTER, BOUND and others) correctly.

## Tests

`tests/test_cpu80286.cpp` runs the hardware sample and the 80286's reset, 24-bit and fault
checks, the shutdown, the PC and segment edits, the interrupt shadow and the snapshot of the
MSW and IDTR. `tests/test_cpu286.cpp` covers the board: the reset through J3-J6, the memory manager,
J2, the sockets, J1, the word fetch, DISASM at the reset address, RESET, the snapshot and the
starts of the Disk 1 and Disk 1A boots.
