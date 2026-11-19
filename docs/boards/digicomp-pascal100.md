# Digicomp Research Pascal-100

**Status:** partial. The board is done: both processors, the two ports, the Standard Map, the I/O
address mirror, MicroEngine memory-mapped I/O, the interrupt paths and jump-on-reset. The
Pascal-100 machine (`examples/pascal`) boots a real UCSD Pascal III.0 system disk and runs the Filer, the editor, the
compiler and the linker. The Extended Map and memory protection are not modeled.

## The real hardware

The Pascal-100 is a dual-processor S-100 CPU set from Digicomp Research Corporation (Ithaca, NY),
1981. It is two cards in two adjacent slots, connected by a cable. It has a Z80 and the Western
Digital Pascal MicroEngine (the WD9000 chip set). The MicroEngine runs UCSD Pascal III.0 P-code
in its microcode. Only one processor runs at a time. The other is in HOLD.

On reset the Z80 has control, so the board starts as a plain Z80 CPU. The Z80 boots the disk and
then starts the MicroEngine. When UCSD Pascal runs, the MicroEngine asks the Z80 to do the disk
and console I/O.

## Sources

| Source | Path | Authority |
|---|---|---|
| Pascal-100 User's Manual, 2nd ed., March 1981 | `Reference/pascal100/manual-*.md` (Claude Projects) | The card: ch. II B-F, ch. IV I-P |
| The distilled reference | `reference/Digicomp Research Pascal-100.md` | A summary of the manual |
| WD9000 chip set data sheet | `Documentation/Pascal 100/WD MicroEngine (bitsavers)/` | The interrupt acknowledge |
| WD UCSD Pascal III.0 OS Reference Manual, Table C-3 | `reference/80-0130016-*.md` | Where the P-machine takes an interrupt |

The manual gives the MicroEngine's interrupt vector as word "FC80". The byte address it also
gives, 1F8C0, is word FC60. The byte address wins: FC60 also agrees with the other two magic
addresses (FC40 = 1F880, FC68 = 1F8D0).

## The Pascal-100 machine

`examples/pascal/pascal.toml` models one real Pascal-100 system, the one used to check this board, as it is in 2026:

- The `pascal100` board (id `cpu0`), flat out (`clock_hz = 0`).
- An Interfacer 1 (id `if1`). Channel A at 00-01 is the console.
- A Disk 1 (id `fd0`) at C0.
- 128K of 16-bit RAM in segments 00 and 01: the box's CompuPro RAM 23. Its 16-bit transfers
  are not verified.
- No IO4. The box had one for a printer and a modem, and its BIOS still has code for it.
- No System Support 1. The box has none, and its BIOS uses no port in 50-5F.

**`examples/pascal/` has a system disk, ready to boot.** UCSD Pascal III.0 is out of print. Its
distribution disks do not boot as they are: manual ch. V D builds the boot track (FBOOT, BIOS, QGEN) for the I/O of each
machine. `builds/pascal-iii/HOWTO-Build-the-System.md` walks through that build. Mount a bootable
system disk for this hardware. To boot the example:

```
cd examples/pascal
pascal100 pascal.toml
```

`pascal.toml` mounts `pascal-dd.dsk` and runs `RUN 0` itself. For a disk of your own, change
the `mount` line, or type `MOUNT fd0:drive0 PASCAL.DSK` and `RUN 0` at the prompt.

The disk is `.dsk` or `.imd`. The Disk 1 takes the UCSD double-density layout: track 0 FM 26 x 128,
tracks 1-76 MFM 15 x 512 (587,008 bytes). Over MCP, start with `run` `from: 0`, because MCP does
not run the machine file's startup.

The acceptance test `acceptance-pascal100-boot` boots the disk in `PASCAL100_BOOT_DISK`. Without
that variable, the test prints SKIP and ctest marks it Skipped. The path must be absolute.

## Register reference

Two write-only ports. The System Control Port is at `port` (switch U225, even, default `D0`).
The Map Output Port is at `port+1`. Both are decoded on A0-A7 only.

| Addr | OUT (write) | IN (read) |
|---|---|---|
| `port` | System Control Port, see below | nothing answers (`FF`) |
| `port+1` | Map Output Port (Z80 only), see below | nothing answers (`FF`) |

**System Control Port** (ch. IV M). The board decodes the bits, not the Table II-1 values:

- DO6 = 0: the processor-select flip-flop U208B gets DO7. 1 selects the MicroEngine, 0 the Z80.
  So `81` also starts the MicroEngine.
- DO6 = 1: latch U237 output DO2-DO0 gets DO7.

| U237 output | On | Off | Function |
|---|---|---|---|
| 0 | `C0` | `40` | Map on for the MicroEngine |
| 1 | `C1` | `41` | Map on for the Z80 |
| 2 | `C2` | `42` | NMI on a protect violation (Extended Map only: kept, no effect) |
| 3 | `C3` | `43` | Auto-transfer: pINT gives the Z80 control |
| 4 | `C4` | `44` | The MicroEngine's interrupt line (`C4` then `44`) |
| 5 | `C5` | `45` | Map half for loading: upper (`C5`) or lower (`45`) |

**Map Output Port** (ch. II D.3, Fig. 2-2). Use `OUT (C),A`:

- The entry is U237 output 5 (as A16) and B bits 7-5 (as A15-A13).
- The entry gets A bits 5-2, **inverted**. Write the one's complement of the page you want.

## How it is simulated

- **Processors.** Unit `z80` is the `CpuZ80` core. Unit `me` is the P-machine core
  (`cpu/cpuPmachine.h`). U208B picks the active one. The run loop follows a switch at the next
  instruction boundary (DESIGN.md 3.0.1). Breakpoints can name a core: `BREAK p100:me <ipc>`.
- **The Standard Map.** It has 16 entries of 4 bits, for 8K pages and 128K of physical memory.
  The Z80 uses entries 0-7 (its A15-A13, A16 held at 0). The MicroEngine uses entries 0-15 (A16-A13
  of its byte address). With the Map off, the address goes to the bus unchanged.
- **The Z80 side.** The board sets the bus's `AddressTranslator` (`core/bus.h`) around the Z80's
  step only. `translate` applies the Map. `ioAddress` puts the port on A0-A7 and A8-A15 (the
  mirror U238, ch. IV K). It also keeps the Z80's own A8-A15 for the Map Output Port. `readData`
  does jump-on-reset.
- **The MicroEngine side.** The board is the core's `WordPort`. Word w is byte 2w (low half)
  and byte 2w+1 (high half). The board asks for one 16-bit cycle (SIXTN). If no board answers, it
  does two byte cycles, even byte first (ch. IV E). Words `FE00`-`FFFF` are I/O and the Map does
  not apply to them. Word `FFpp` is an 8-bit cycle to port pp, with pp on both address halves.
  Word `FEpp` asks for 16 bits. No board here answers SIXTN on an I/O cycle, so `FEpp` is the
  same 8-bit cycle. A read puts the byte in both halves of the word.
- **Interrupts** (ch. II F, ch. IV O). pINT goes to the Z80's IRQ and NMI to the Z80's NMI. An
  NMI always resets U208B. A pINT resets it when auto-transfer is on. Both are levels: while
  one is up, the MicroEngine cannot keep control. `C4` then `44` sets a request that the board
  holds until the MicroEngine acknowledges it.
- **The MicroEngine's acknowledge.** It writes word `FC40` and reads the vector at word `FC60`.
  Then it signals the semaphore. Table C-3 takes an interrupt only when no task is ready
  (label 6).
- **Debugger.** History, REGS and STEP show the MicroEngine's operators. The board's
  `peekCodeByte` reads code byte SEGB*2 + IPC through the Map. On the MicroEngine, NEXT is a
  STEP. `RESET CPU` resets the active processor only and names it.

Properties: `port`, `jump_on_reset` (J202), `reset_addr` (U242), `clock_hz` (the U102 header;
0 = flat out), `idle`, `achieved_hz`. The board works on the IEEE 696 bus only.

### Reset

- `Reset::PowerOn`: the Map contents become 0. Then everything that `Reset::Bus` does.
- `Reset::Bus`: both cores reset, the Z80 gets control, every U237 output goes off, the
  MicroEngine's request clears, and jump-on-reset arms. The Map keeps its contents. The
  MicroEngine runs its boot from word `FC68` when it next gets control (ch. II C.3).

## Quirks reproduced

| Quirk | If you get it wrong |
|---|---|
| The Map stores its data inverted | Every page that SBQB loads goes to the wrong place |
| The Map entry comes from B, before the mirror | The Map port loads the wrong entry |
| The System Control Port ignores A8-A15 | QB's `OUT (C),A` with any B does not switch processors |
| The port is decoded by bits (`81` = `80`) | Software that writes other values does not switch |
| `C4` then `44` is held until acknowledged | The MicroEngine never sees the interrupt |
| Jump-on-reset replaces the data, the cycles still go out | A `MEM R 0000` breakpoint does not fire |

## Limitations and deliberate departures

- **The Extended Map and memory protection** (J201, the `C2` NMI) are not modeled. The `C2` bit
  is kept and does nothing. Only software for the Extended Map would notice.
- **PHANTOM above 64K** (J203) and **the J105 I/O wait state** are not modeled. Both are off
  in the factory settings.
- **The FC40 data is not known.** The data sheet says the acknowledge puts a 4-bit level code on
  DAL0-3. The manuals do not say which level the Pascal-100 uses or what the other bits are. The
  board writes `FFFF`. The manual tells the programmer to keep nothing at FC40.
- **The read of an 8-bit I/O port** puts the byte in both halves of the word. This is read from
  the buffer table in ch. IV I.1. It is not checked on a real board yet.
- **Byte operators** read and write the whole word. Whether the WD9000 does real byte memory
  cycles is not settled. So a byte store into the I/O window (words `FE00`-`FFFF`) reads the
  port before it writes it. A read can take a character from a UART. III.0 does its port I/O
  with word pointers (`PORTIO`), so this case is not expected.
- **A switch happens between operators.** On the real board a hold can stop the MicroEngine in
  the middle of an operator (DESIGN.md 3.0.1).
- **The Map contents at power-up** are 0 here. On the real board they are undefined.
- **NEXT on the MicroEngine** is a STEP. A step-over for P-code needs a target that knows the
  segment.
- **`RESET CPU`** resets the processor only. It does not arm jump-on-reset and does not clear
  U237, so a Z80 with its Map on starts at logical 0000 through the Map. Use `RESET` for the
  reset of the real board.
- **The owner's box runs at 3.0 MHz.** The manual has headers for 2 and 2.5 MHz. Measured with a
  25,165,824 T-state Z80 loop: 2.997 to 3.008 MHz. With `clock_hz = 3000000`, Z80 timing is exact,
  and P-code runs close to the box. Calls and loops are 9 to 11% slower than the box:
  see `docs/devguide/pmachine.md`.
- **Speed.** Flat out on an Apple silicon Mac, the boot to the III.0 command line takes 0.09 s,
  and the compile of a 1281-line program takes 1.8 s. The history records every operator. A
  profile of that compile, taken before the core had its own `captureRegs`, gave `step()` 34% of
  the run thread, the register copy 38% and the eight code peeks for each operator 13%. With
  `captureRegs` the compile took 2.75 s before and 1.78 s after.
- **The disk does not turn.** The uPD765 model finds a sector at once and gives "missing address
  mark" at once (`chips/upd765.h`). On the real box, a Pascal BIOS that waits for the end of a
  command with no timeout hangs when drive B holds a single-density disk. In the simulator the
  same BIOS and disks work.
- **DISASM `<addr>`** reads physical bytes, as on every machine. It does not follow SEGB or the Map.

## Verification

`tests/test_pascal100.cpp` (148 checks):

- Every Table II-1 value, the bit decode, the reset state and the straps.
- Map loads (entry, half, inversion), the Z80 and the MicroEngine through the Map, the mirror,
  and jump-on-reset.
- MicroEngine words with 8-bit and 16-bit memory, the I/O window, the interrupt, and pINT and
  NMI switching.
- **The gate.** A Z80 program loads the Map as the soft boot does and writes the init vector at
  38D0 (= 1F8D0). Then it starts the MicroEngine. The MicroEngine boots, runs a P-code program and
  gives control back with word `FFD0`. The Z80 continues after its OUT. The test also checks
  `BREAK p100:me`, SNAPSHOT/RESTORE during the MicroEngine's run, HISTORY CPU and REGS.
- A cycle breakpoint that stops a read and runs the instruction again: the jump-on-reset count
  and the MicroEngine's interrupt request come back as they were before the read.

Each rule was also checked by a deliberate break of the code: every break made a test fail.

**With the real software** (Stage 5, 2026-11-03). The disk is the working III.0 system disk of that real system:

- The boot, the Filer `V` and `L`, and the run-time error screens are the same as on the real box.
  The broken paths of III.0's error handler after a run-time error are the same too.
- A test program printed the same numbers on both machines: `MODI` of a negative number, real
  rounding, and string compare.
- The compiler and the linker made the code file of a 1281-line program (`USES LONGINT`). It is
  the same, byte for byte, as the one the real machine made in 1983.
- The user's FORMAT program drives the Disk 1 from Pascal through the I/O words. It formatted all
  77 tracks of a blank disk to the UCSD double-density layout.

Two P-machine faults were found this way and fixed (reference 7.12): the stack overflow test of
`createmscw`, and the error number of `MODI`.

## References

- Pascal-100 User's Manual (Digicomp Research, 1981).
- WD9000 Pascal MicroEngine chip set data sheet (Western Digital).
