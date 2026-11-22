# CLK x86 core, vendored

The 8088 in pascal100 (`src/cpu/cpu8088.*`) runs on the x86 instruction-set core of
Clock Signal (CLK) by Thomas Harte: https://github.com/TomHarte/CLK, MIT License
(`LICENCE` in this folder, which must stay with these files).

- Upstream commit: 5eae9b61015221b53135de6128bf4e2bbdb78dad (2026-10-30).
- Only the files the 8086 model needs, at their upstream paths so their own `#include`s
  work: `InstructionSets/x86/`, `Machines/PCCompatible/{Segments,SegmentedMemory,
  LinearMemory}.hpp`, `Machines/Utility/MemoryFuzzer.hpp`, `Numeric/{Carry,RegisterSizes}.hpp`.
- Built as a SYSTEM include directory, so CLK's own warnings do not fail pascal100's
  warnings-as-errors build.

## Our changes (each marked `pascal100` or described here)

Found by running the SingleStepTests 8088 v2 hardware suite (3,007,000 tests); with these,
every test gives the real chip's registers and RAM. Not yet offered upstream.

1. `Implementation/Repetition.hpp`, CMPS: the 8086 path reads DS:SI before ES:DI, as the
   real chip does (upstream read ES:DI first; the values were right, the bus order wrong).
2. `Implementation/Arithmetic.hpp`, IDIV: the 8086/8088 also takes the divide error on the
   most negative quotient (80h for a byte); upstream used the 80286 rule.
3. `Implementation/BCD.hpp`, AAM 0: the 8088 sets SF, ZF and PF as for a zero result before
   the divide error.
4. Missing standard includes: `<array>` and `<functional>` in `Descriptors.hpp`, `<limits>`
   in `Arithmetic.hpp`.

The 80286 (`src/cpu/cpu80286.*`, model `i80286`) was checked against the SingleStepTests
80286 v1_real_mode suite (about 1.48 M tests from a real Harris 80C286-12). Changes 5 and 6
apply to the 80286 model only. With them, 99.997% of the tests give the real chip's
registers and RAM. Changes 7 and 8 came from a code review.

5. `Implementation/LoadStore.hpp`, LEA: a register operand (mod 11) is an invalid opcode
   (INT 6) from the 80286 on.
6. `Implementation/PerformImplementation.hpp`, `interrupt()` in real mode: the 80286 pushes
   FLAGS, CS and IP first, then reads the vector. Upstream read the vector first (the
   values were right, the bus order wrong).
7. `Implementation/PerformImplementation.hpp`, `fault()`: a second exception while the first
   is delivered is not caught. It leaves `perform()`, and pascal100 shuts the 80286 down
   (`src/cpu/x86_impl.h`). Upstream caught it, printed "DOUBLE FAULT TODO!" to stdout (the
   JSON-RPC stream under `--mcp`) and ran on.
8. `Implementation/PerformImplementation.hpp`, `source_indirect()` (LGDT, LIDT, SGDT, SIDT):
   the operand's address goes through the switch on its form. Upstream always used the
   base + index form, so `[disp16]` and `[BX]` gave a wrong address. The hardware suite has
   no test of these instructions.

The 49 tests that still fail are corner cases: an instruction longer than 10 bytes (2),
ENTER with a large level (1), AAM 0 (9), the IDIV quotient limit (4), and ESC with no 80287
(33).

The spike that chose this core, with its results, is outside the repository
(`Pascal100-Altairsim/clk-8088-spike/` in the project folder).
