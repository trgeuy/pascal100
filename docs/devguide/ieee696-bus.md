# The IEEE 696 bus

pascal100 has one bus: IEEE Std 696-1983. altairsim, the program pascal100 began as, also has
the Altair bus of 1975; pascal100 took that bus out in standalone step 3b (2026-11-09), and the
`bus` key in `[machine]` on 2026-11-18. The Monitor guide describes the addresses that the
operator sees (*Addresses: 24 bits on the bus*).

This page is for a board author. It tells you what to do, for each kind of board, so that the
board works on the IEEE 696 bus. For most boards, the answer is: nothing.

Read these too:

- `reference/IEEE_696_1983.md`: the distillation of the standard, with the pin list.
- `DESIGN.md` §4.0: altairsim's reasons for the Altair bus, and pascal100's note on why it
  changed.
- `src/core/bus.h` and `src/core/board.h`: the comments on each function are the contract.

## What the bus carries

| | IEEE 696 bus |
|---|---|
| Memory address | A0-A23: 256 segments of 64K |
| I/O address | A0-A15. The processor drives A8-A15. Boards decode A0-A7. |
| Memory transfer | 8 bits, or 16 bits when the slave answers SIXTN\* |
| `Bus::addrMask()` | `0xFFFFFF` |

`BusCycle` carries the IEEE 696 parts in three fields:

- `addr` is A0-A15. `ext` is A16-A23. `phys()` gives the 24-bit address.
- `xtrq` is true in a 16-bit transfer (sXTRQ\*). `data` is the even byte, and `dataHi` is the
  odd byte.
- `port()` is A0-A7 of an I/O cycle.

## An ordinary board: do nothing

A board that reads only `c.addr` ignores A16-A23. On the IEEE 696 bus it answers in every 64K
segment. This is correct: a real board that does not decode A16-A23 does the same. The
standard calls it a board that does not decode the extended address.

**Decode a port from A0-A7 only.** Use `c.port()`, not `c.addr`, in an I/O cycle. On the IEEE
696 bus `c.addr` has the byte that the processor drove on A8-A15: B or A on a Z80, the port
again on an 8085. The bus keeps one decode table of 256 ports. A board that decodes A8-A15
breaks that table, and `Bus::setVerify(true)` aborts with `BUS DECODE CACHE IS STALE`.

## A board that decodes A16-A23

Do these four things. The `memory` board (`src/boards/s100-memory.{h,cpp}`) with `ext = true`
is the example.

1. Return `true` from `decodesExtended()`. The bus then builds a decode table for each segment
   that is used. A board that decodes `c.ext` and returns `false` gets segment 0's table in
   every segment, and `setVerify(true)` aborts.
2. Check `c.ext` in `decodes()` and in `assertsPhantom()`.
3. Override `peekX(phys, out)` when the board holds different bytes in different segments,
   as the `memory` board does: `peek(addr, out)` alone sees only A0-A15.
4. Give `memMap()` ranges as 24-bit bus addresses. `Machine::segmentsOf()` asks `decodes()`
   at both ends of each range, and `SHOW BUS MAP` cuts a range at each segment boundary. A
   board without `decodesExtended()` gives A0-A15 ranges, and the map marks them "all
   segments".

## A 16-bit slave

Override three functions:

- `answersSixtn(c)`: return `true` when the board answers a 16-bit transfer at `c`.
- `read16(c, even, odd)`: give the byte at the even address `c.addr` and the byte at `c.addr | 1`.
- `write16(c)`: store `c.data` at the even address and `c.dataHi` at the odd address.

The bus calls them only for an even address. The `memory` board's
`sixteen_bit` property is the example.

## The registry-wide test

The `every board type` section of `tests/test_ieee696.cpp` adds every registry type to a machine.
It checks that each board decodes its ports from A0-A7 and that a board without
`decodesExtended()` answers alike in every segment.

## A DMA master

Use `memReadX(phys)` and `memWriteX(phys, data)` with the full 24-bit address. They are
physical: an address translator never changes them. The Disk 1 (`CompuProDiskBoard::transferOne`) is the
example.

A DMA board gives its TMA0\*-3\* priority in `tmaPriority()`, 0-15 (IEEE 696 2.8.3). Read it
from the board's switches. The default, -1, means that the board has no TMA lines.

At each instruction boundary, `Debugger::serviceDma()` gives the bus to the requester with the
highest priority. That board keeps the bus until it drops pHOLD. Then the next requester gets
the bus. Each board gets one grant per boundary.

`Machine::tmaConflict()` refuses two DMA boards with the same priority. It also refuses a board
with no TMA lines beside another DMA board. The TOML loader and the run loop both use it.

## A 16-bit master

Call `memRead16(phys)` or `memWrite16(phys, even, odd)`. When no slave answers SIXTN\*, these
functions run no cycle. `memRead16` returns `sixtn == false`, and `memWrite16` returns `false`.
The master then moves the two bytes itself. The standard does not give the order of the two
bytes, so the master chooses it: `byteSerialRead16(phys, evenFirst)` and
`byteSerialWrite16(phys, even, odd, evenFirst)` do the two byte cycles in that order.

## An address translator

A processor board with its own memory map (the Pascal-100) implements
`AddressTranslator::translate(logical, cycle)`. Set it with a `TranslatorScope` around the
step of its own core:

```cpp
TranslatorScope scope(bus, &map_);   // memRead/memWrite go through map_
core_.step();
// the scope restores the previous translator, also when step() throws
```

Only `memRead()` and `memWrite()`, the processor's logical cycles, go through the translator.
The monitor, DMA and the `X` functions are physical and never go through it.

## Speed

Segment 0 keeps its old code path, `Bus::memRead0`/`memWrite0`. Do not route segment 0 through
the general segment path (`readAt`): that measured 3-5% slower on ZEXALL.

**Keep the translated path out of line.** `memRead` and `memWrite` are inline in `bus.h`, so
they go into every memory access of every CPU core's `step()`. They contain only the test for a
null translator. The translated path is in `memReadMapped`/`memWriteMapped`, defined in `bus.cpp`, so the
compiler cannot inline it into another file. The build has no link-time optimization. If it
gets it, measure again. Do not mark the functions with a compiler-specific attribute: the test
for `_MSC_VER` that this needs fails the platform lint (DESIGN.md 2.1).

When the translated path was written inline, the compiler put it into each of those sites. A
machine without a translator never takes that path, but its run loop got slower. ZEXALL did not show it,
because it calls `step()` directly and not through the run loop. The benchmark that shows it is
an 8080 loop of 67,109,634 instructions through the MCP `run` tool (`Debugger::run`), flat out,
RelWithDebInfo, with the builds run in alternation (2026-11-03):

| Build | Against altairsim `290088e` |
|---|---|
| The translated path inline (`e36375a`) | +9.5% to +12.8% (five sessions) |
| The translated path out of line | +2.5%, +4.6% and +5.2% (three sessions) |

The rest is not explained. A profile shows the bus-history observer up by half since Stage 2,
which made `BusCycle` larger. That is a candidate, not a finding.

On the Pascal-100 the Z80 takes the translated path on every access. There, the out-of-line
form is not slower: the FIT compile (1281 lines) took 1.72 s against 1.79 s inline (5 runs
each).

## Tests

- `tests/test_ieee696.cpp`: the bus core, the `memory` settings, the monitor and MCP address
  forms, Intel HEX type 04, and the registry-wide test. `tests/test_memory.cpp`
  (`memory_segments`) covers a board of more than 64K. It also boots CP/M 2.2N under
  `setVerify(true)`: that catches a board that decodes A8-A15.
- `tests/test_disk1.cpp`: DMA to `010100`, with all 24 address bits, and a `BREAK MEM` on that
  physical address.
- `tests/media/ieee696/`: two test machines. `cpm22n-alias64k.toml` has 64K that answers in
  every segment. `cpm22n-seg128k.toml` has 128K of 16-bit memory in segments 00 and 01
  (`ext = true`), set up like a CompuPro RAM 23.
- `acceptance-ieee696` (ctest): boots both machines and runs DIR, STAT and a PIP copy.

The CLI acceptance test cannot catch a wrong port decode, because the bus does not ask
`decodes()` again after it builds the table. Use `setVerify(true)` in a unit test for that.
