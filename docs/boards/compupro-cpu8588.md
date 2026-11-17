# CompuPro CPU 8085/88

**Status:** partial. The swap port, the Memory Manager, power-on-jump, reset-on-swap and the IOW
wait state work. The 8085's TRAP and RST 5.5/6.5/7.5 inputs are not connected (see Limitations).

## The real hardware

The CPU 8085/88 (CompuPro division of Godbout Electronics, board 161F, manual dated 2/83) is an
IEEE 696 processor board with two processors: an 8085A-2 (U41) and an 8088 (U20). Only one
processor is on line at a time. The board holds the other processor on its HOLD line.

Software swaps the processors with an IN from one I/O port. An OUT to the same port sets the
Memory Manager, a latch that drives the upper address lines. The 8085 can then reach all 16 MB
of the IEEE 696 address space. The 8088 drives A0-A19 itself and gets A20-A23 from the latch.

The 8085 runs at 6 or 2 MHz (paddle switch S4). The 8088 runs at 8 MHz from an 8284 clock
generator. The parts list gives other crystals, for 5 MHz on both processors (see Sources).

Configuration in period:

- **S1:** eight options. XA3 (1), XAC (2), IOW (3), 5RS (4), 8RS (5), JOR (6), MW (7), POJ (8).
- **S2:** A8-A15 of the 8085's power-on-jump page. ON is 1.
- **S3:** A0-A7 of the port. ON is 0. The CompuPro standard is FD.
- **S4:** the 8085 clock, 2 MHz (left) or 6 MHz (right).
- **J6:** install it only for an IMSAI front panel. **J7:** pads A-B (factory) let RESET\* reset
  the 8088 too. Pads B-C reset the 8088 only at power-on.

## Sources

| Source | Path | Authority |
|---|---|---|
| *CPU 8085/88 Technical Manual*, CompuPro/Godbout, 161F, 2/83, 31 pp. with a six-sheet schematic | `reference/CompuPro CPU 8085-88.md` | Every fact on this page. Switches pp. 4-10, Memory Manager pp. 10-11 and 18, swap pp. 11-12 and 16-18, power-on-jump pp. 18-19, schematic pp. 24-29 |

The manual contradicts itself on the crystals. The text and the schematic give 24 MHz (X2) and
12 MHz (X3), for an 8 MHz 8088 and a 6 MHz 8085. The parts list gives 15 MHz and 10 MHz. The
clock properties take any rate, so a machine file can set either build.

The manual and the schematic also differ on a reset-on-swap with power-on-jump. The text (p.12)
says the 8085 does the jump each time it comes on line. The schematic (sheet 5) arms the jump
from the 8085's RESET OUT only through S1-6 (JOR). The simulation follows the schematic. With
the quick-start settings (JOR on) the two readings give the same result.

## Register reference

One port, at `port` (S3, default FD). The board compares A0-A7 only.

| Addr | OUT (write) | IN (read) |
|---|---|---|
| `port` | Latches the Memory Manager byte (U28). The 8085 puts all 8 bits on A16-A23. The 8088 puts bits 7-4 on A20-A23. | Swaps the processors. Reads FF: nothing drives the data bus. |

## How it is simulated

- **Two cores, one board.** The board is a `CpuCard` with two units, `8085` and `8088`. Only the
  active core steps, so only the processor on line sees pINT and NMI.
- **The swap.** The IN cycle changes the active core at once. The run loop runs the other
  processor from the next instruction boundary. The real board finishes the bus cycle, then
  trades HOLD and HLDA. Neither way cuts an instruction in two.
- **Coming on line.** A processor resumes where it stopped. With `swap_reset_8085` or
  `swap_reset_8088` on, the board resets it first. The 8088 waits in its reset state from
  power-on, so its first time on line starts at FFFF0.
- **The Memory Manager.** For the 8085, an address translator adds the latch as A16-A23 to each
  memory cycle. For the 8088, the board gives the latch's high nibble to the core as A20-A23.
- **Power-on-jump.** The translator replaces the data of the 8085's first three reads with C3,
  00 and the S2 page. The bus cycles still go out.
- **IOW.** Each I/O cycle of either processor adds one wait state to the bus.
- **The clock.** At each swap the board sets the clock to the rate of the processor on line.
- **The debugger.** The PC of the 8088 is the linear address CS\*16+IP. A PC breakpoint on the
  8088 matches that address. Messages show the PC as `CS:IP (linear)`.
- No DMA. The board decodes only its port.

Properties:

| Property | Switch | Default | Meaning |
|---|---|---|---|
| `port` | S3 | FD | The swap and Memory Manager port |
| `xac` | S1-2 | on | RESET\* clears the Memory Manager |
| `iow` | S1-3 | on | One wait state in every I/O cycle |
| `swap_reset_8085` | S1-4 5RS | off | Reset the 8085 each time it comes on line |
| `swap_reset_8088` | S1-5 8RS | off | Reset the 8088 each time it comes on line |
| `jump_on_reset` | S1-6 JOR | on | Power-on-jump after RESET\* too |
| `power_on_jump` | S1-8 POJ | off | The 8085 starts with a JMP to `poj_addr` |
| `poj_addr` | S2 | 0000 | The power-on-jump page (A8-A15) |
| `reset_8088` | J7 | on | RESET\* resets the 8088 too |
| `clock_hz_8085` | S4 | 0 | The 8085 clock. 0 runs flat out |
| `clock_hz_8088` | X2 | 0 | The 8088 clock. 0 runs flat out |
| `idle`, `achieved_hz` | | | The run policy, as on the other CPU boards |

The switch defaults are the manual's quick-start settings (p.4).

### Reset

- `Reset::PowerOn` (POC\*): the board resets both processors and clears the latch. The 8085
  goes on line. With `power_on_jump` on, the jump is armed.
- `Reset::Bus` (RESET\*): the board resets the 8085. It also resets the 8088 when `reset_8088`
  is on. It clears the latch when `xac` is on. The 8085 goes on line. With `power_on_jump` and
  `jump_on_reset` on, the jump is armed.

## Quirks reproduced

| Quirk | If you get it wrong |
|---|---|
| The swap IN reads FF into A | A program that keeps A over a swap loses it. The manual tells the programmer to PUSH it. |
| The 8088 uses only the latch's high nibble | 8088 memory above 1 MB lands in the wrong place. |
| The full latch byte stays latched while the 8088 runs | The 8085 comes back in the wrong segment. |
| An OUT to the port moves the 8085's own next fetch | 8085 code that changes the latch must continue in the new segment. |
| Each processor resumes where it stopped (5RS, 8RS off) | A two-processor program, such as the manual's example on p.12, restarts at the wrong place. |

## Limitations and deliberate departures

- **The 8085's TRAP and RST 5.5/6.5/7.5 are not connected.** On the real board NMI\* goes to
  TRAP, and VI0\*-VI2\* go to RST 5.5/6.5/7.5. The 8085 core does not wire those pins yet. pINT
  reaches the 8085 on INTR. Software that uses those 8085 interrupts does not work.
- **Not modeled:** MWRITE (S1-7), the IMSAI front panel (J2, J6), and XA3 (S1-1). The bus model has
  no ADSB\* line.
- **The reset-on-swap pulse** is not timed. The real board gives a pulse of about 2 us.
- **The status PROM (U30)** is not modeled. Each core gives its own status bits to the bus.
- **The 8088 disassembler** (`src/isa/isa8088.cpp`) uses the instruction decoder of Clock Signal,
  the decoder that the 8088 core runs on, and its own text. Addresses are `SEG:OFF`. `NEXT`
  steps over `CALL` and `INT` on the 8088. There is no 8088 assembler, so `EDIT` takes only
  bytes on the 8088.

## Verification

`tests/test_cpu8588.cpp`:

- The 8085 starts at 0000 and the 8088 waits at FFFF0.
- IN FD swaps. Each processor resumes after its IN, and the IN reads FF.
- With `swap_reset_8088`, the 8088 starts at FFFF0 each time.
- An OUT FD moves the 8085's stores to the latched segment.
- With the latch at 1F, the 8088 stores at 1xxxxx, not at 1Fxxxx.
- Power-on-jump to E000. After RESET\*, `xac` clears the latch and `jump_on_reset` jumps again.
- `iow` adds one T-state to an OUT.
- A PC breakpoint at the 8088's linear address stops there, with the 8088 on line.

No period software has run on it yet. The next check is CP/M-86.

## References

- `reference/CompuPro CPU 8085-88.md`
- `docs/boards/digicomp-pascal100.md`, the other two-processor board.
