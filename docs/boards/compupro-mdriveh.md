# CompuPro M-Drive/H

**Status:** done. The port map, the shifting address counter, the cascade of up to eight
boards and the floating bus for an absent board are modeled. Wait states and the PWRFAIL*
jumper are not (see Limitations).

## The real hardware

The **M-Drive/H** (board 197B, CompuPro, a Godbout company, 1982) is an IEEE 696 / S-100 board
with **512K bytes of dynamic RAM that the system uses as a disk drive**, drive M:. It takes no
memory space: the computer sees two I/O ports. One loads a starting address into a 22-bit
counter, the other reads or writes the byte at that address, and the counter counts up after
each byte. "The /H stands for Hardware": the earlier M-Drive was software that used the CPU
8085/88's extended memory.

**Up to eight boards share the same two ports.** The top three bits of the counter pick the
board, so the software sees one array of up to 4 megabytes. The RAM is lost at power-off.
CompuPro's CP/M 2.2 (2.2MDB or later), CP/M-86 (1.1PAA or later) and MP/M 8-16 (2.1D or later)
have the driver built in: at cold boot they count the boards, print the size and format drive M:.

Chips: 64 x 4864 (64K x 1) DRAM in 8 rows, an Intel 8203-3 DRAM controller with a 24 MHz
crystal, six 74LS161/163 counters for the address, a 25LS2521 port comparator, a 74LS266 for the
cascade compare, and two PALs (197P-1 port decode, 197P-2 row select).

One 10-position DIP switch, S1: positions 1-7 set the port (A7-A1), positions 8-10 the cascade
address. "Markings on switch, not legend."

## Sources

| Source | Path | Authority |
|---|---|---|
| CompuPro *M-Drive/H Technical Manual*, 3/83, 21 pp. | `reference/CompuPro M-Drive H.md` | Ports, S1, the schematic (counter wiring p. 17), the sample BIOS (pp. 11-16) |
| CompuPro CP/M 2.2N BIOS source, text left in unused sectors | `tests/media/compupro/CPM22N.DSK` | The sizing routine (`HMDTEST`): read, write back, read |
| CompuPro CP/M-86 1.1, MP/M 8-16 2.1F, CP/M 2.2N | `examples/cpm86`, `examples/mpm8-16`, `tests/media/compupro` | The shipped drivers: what they print and do with 0, 1 and 2 boards |

The manual does not publish the equations of the port PAL (U6, 197P-1). Two behaviours below
come from the design, not from a statement, and the reference marks them ⚠.

## Register reference

`port` is the data port; the address port is the next one. CompuPro's standard is C6H/C7H.

| Addr | OUT (write) | IN (read) |
|---|---|---|
| `port` + 0 | store the byte at the counter (selected board only), then counter + 1 | the byte at the counter (selected board only; no board: FF), then counter + 1 |
| `port` + 1 | counter = (counter << 8 \| byte) mod 2^22 | not decoded (floats to FF) |

**The address load shifts.** Each counter chip loads the outputs eight bits below it, and the
lowest two load the data bus (schematic p. 17). So three OUTs, high byte first, give a 22-bit
address, and the top two bits of the first byte are lost. Software (the manual's `SETMDRV`)
sends A, H, L.

**Board select.** MA21-MA19 are compared with S1-8..10 (ON = 0). The board whose `cascade`
matches stores writes and drives reads.

**The software's layout** (reference §5): byte address = (sector - 1) x 64K + track x 128, 128-byte
sectors, 8 sectors per board, and an 8-bit parity sum per sector kept in the first 512 bytes of
each 64K (the 4 reserved tracks). Parity is software only; the board stores 8 bits per byte.

## How it is simulated

- **Decode:** an I/O read of `port`; an I/O write of `port` or `port` + 1. Nothing else.
- **Shared ports (DESIGN.md 4.6):** the board answers `sharesPort()` true for both ports. So
  two or more M-Drive/H boards that take the same OUT are not reported as contention, and the
  bus never caches these ports. On a read, `drivesBits()` is FF for the selected board and 00
  for the others. With no board selected nothing drives the bus, and the read floats to FF.
  CompuPro's sizing routine depends on that FF.
- **Every board loads and counts** on every cycle at its ports, selected or not, so the
  counters of all the boards stay equal and a transfer that crosses a 512K boundary goes on to
  the next board.
- **RAM:** 512K in a `std::vector`. **No `pump()`, no timers, no interrupts, no DMA.**
- **Properties:**

| Property | Default | Meaning |
|---|---|---|
| `port` | `C6` | S1-1..7 (A7-A1). Must be even: A0 picks data or address |
| `cascade` | `0` | S1-8..10: which 512K of the 4M range, 0-7. The 1st board is 0 |
| `fill` | `random` | RAM at power-on: `random` or `zero` |
| `seed` | `1` | The seed for `fill = random`: the same seed fills the same way at every power |

- **Snapshot:** the counter and the 512K.

### Reset

- `Reset::PowerOn` (POC*, cold): the RAM is filled per `fill` and `seed`; the counter goes
  to 0. (The real counter's power-up state is not defined; software always loads it first.)
- `Reset::Bus` (RESET*, warm): nothing. The counters' clear inputs are tied high, and DRAM is
  not cleared by a reset. A system that is reset keeps the files on drive M:.

## Quirks reproduced

| Quirk | If you get it wrong |
|---|---|
| A range with no board reads FF | CompuPro's sizing counts boards that are not there: the drive is too big, and files written to it are lost |
| Every board counts every byte | With two boards, a sector that crosses 512K comes from the wrong place on the 2nd board |
| The address port shifts | A driver that sends three bytes loads a wrong address if the load replaces instead of shifting |
| RAM at power-up is not zero and not FF | A driver that trusts the first read is not tested; CompuPro's writes before it trusts |
| RESET keeps the RAM | After a reset the system formats M: again (the BIOS does that at cold boot), but a program that reads M: after a warm start would find it empty |

## Limitations and deliberate departures

- **No wait states.** The real board holds RDY (pin 72) while the 8203 is busy. A simulated
  transfer takes no extra time. No software we know of measures it.
- **PWRFAIL* (jumper J1) is not modeled.** It goes to the port PAL; the manual does not say
  what it does. Probably it blocks writes while power fails.
- **The PAL is inferred.** "Every board loads and counts, only the selected one drives" and
  "the address port is write-only" are taken from the design (reference §4). CompuPro's own
  software works with both, with one and with two boards.
- **The counter starts at 0 at power-up**, not at a random value.
- **DRAM refresh, parity errors and failing chips** are not modeled: the RAM never loses a bit.

## Verification

`tests/test_mdriveh.cpp` drives real bus cycles: the shifting load (and the fourth byte that
pushes the first out), counting, the floating bus past 512K and at 3FFFFF -> 0, the port move,
two boards with a transfer across the boundary, both contention rules (two boards on one
`cascade` contend on a read; an Interfacer 1 at C6 contends on a write), the manual's sample
driver with its parity, the CP/M 2.2N sizing routine (2 boards, 1 board with random RAM, and 0
when the only board is strapped as the 2nd), reset, power, `fill`, `seed`, a snapshot, and the
property checks.

`tests/acceptance/mdriveh-cpm22n.cmake` boots CompuPro CP/M 2.2N with one board: the sign-on
says `1 M-Drive/H`, the system formats M:, and `PIP [V]` copies and verifies a file there.

By hand, on 2026-11-19:

| System | Boards | Result |
|---|---|---|
| CP/M 2.2N | 0 | `0 M-Drive/H`; `MFORM M` says `DRIVE SPECIFIED IS NOT A MEMORY DRIVE` |
| CP/M 2.2N | 1 | `1 M-Drive/H`; formats M:; `PIP M:=A:*.COM[V]` 35 files; `M:STAT M:DSK:` 508K, 8 sectors per track, 4 reserved tracks |
| CP/M-86 1.1PD | 1 | `512K M-DRIVE/H Active as Drive "M".`; `PIP [V]` 13 files; `M:STAT`; `MFORM M` asks and formats |
| CP/M-86 1.1PD | 2 | `1024K M-DRIVE/H Active`; `STAT M:DSK:` 1016K, 16 records per track |
| MP/M 8-16 2.1F | 1 | `512K M-DRIVE/H Active`; formats M:; `PIP M:=A:*.*[V]` 14 files |

## References

- `reference/CompuPro M-Drive H.md` — the distilled manual, with the sizing routine.
- DESIGN.md 4.6 — contention, `drivesBits()` and `sharesPort()`.
