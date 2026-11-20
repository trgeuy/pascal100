# 80286 hardware test sample

`sample.txt` holds 12 tests from each of the 326 instruction-form files of the **SingleStepTests
80286 v1_real_mode** suite by Daniel Balsom (MIT licence, https://github.com/SingleStepTests/80286,
version 1.1.0), recorded from a real Harris N80C286-12 through an ArduinoX86 interface. The full
suite (about 1.48 M tests, 311 MB) is not in the repository; the run against all of it is described
in `docs/sources.md` (the x86 exception) and in the project folder.

The 12 are spread evenly through each file. `tests/test_cpu80286.cpp` runs them through `Cpu80286`
on a real `Bus` with a `memory` board covering 000000-10FFFF (real mode reaches 10FFEF).

## Format

The same as the 8088 sample (`../8088/README.md`), with these differences:

- Addresses in `R` and `W` are up to 24 bits.
- The suite ends each test with a HALT and records IP one past it. `F` here holds the IP after the
  instruction (the suite's IP - 1), except for HLT (`F4`), which is its own terminating HALT.
- `Q e c`: `e` is the exception the instruction raised (`-1` = none), `c` the real cycle count. The
  suite does not exercise the prefetch queue, IF or TF.

The unit test also checks the time against `c`. The suite starts each test after a jump, with an
empty queue, and ends it with HLT; this adds about 11 clocks to every count. To see the result per
instruction form on a larger sample, set `P100_286_TIMING` to a file in this format; set
`P100_286_DUMP` to a form name (such as `A5`) to list its tests one by one.
