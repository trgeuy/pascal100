# Contributing to pascal100

pascal100 is maintained by Timothy Geuy (`trgeuy` on GitHub). If you find a problem, open an issue.
Give the exact command, the machine file and the output. A pull request is welcome, but open an
issue first, so that we can agree on the change.

pascal100 is not part of altairsim. Do not report pascal100 problems to the altairsim project.

## Build

You need a C++20 compiler and CMake 3.20 or later. SDL3 is optional, and CMake finds it if it is
installed.

```sh
cmake -B build
cmake --build build -j
```

## Test

```sh
./build/altair_tests <names>        # one or more unit suites, e.g. ./build/altair_tests pascal100 disk1
./build/altair_tests --list         # the suite names
ctest --test-dir build -LE slow     # all tests except the slow CPU exercisers
ctest --test-dir build              # all tests
```

Read the pass line, `100% tests passed out of N`. The absence of the word "error" is not enough.

The test `acceptance-pascal100-boot` needs a Pascal-100 system disk, which the repository does not
hold. Set `PASCAL100_BOOT_DISK` to a copy of your disk to run it. Without it, the test is skipped.

CI (GitHub Actions) builds on Linux (GCC) and Windows (MSVC) with warnings as errors, and runs the
fast tests. To get the same warning check before you push:

```sh
cmake -B build -DWERROR=on && cmake --build build -j
```

## Code

- Match the style of the code around your change: naming, indentation, comments and idiom.
- The word is **board**, not card, for the object, the command and the table (`DESIGN.md` 0.3).
- Do not give hardware a behavior that it never had, to fix a software symptom. Look at the host,
  filter and monitor layers first.
- `docs/manual/ref/` is generated from the binary. Change the generator and run
  `cmake --build build --target docs-reference`. Do not edit those `.md` files by hand.
- The C++ namespace is `altair` and the internal targets are `altair_*`. They keep their names, so
  that fixes from altairsim can still be cherry-picked.

## Reference documents

Each board is modeled from primary sources: the original manuals, data sheets and schematics.

- The scanned manuals are not in the repository. They are large, and they are not ours to give
  out. `reference/` holds a text summary of each source: register maps, ports, bit tables,
  geometry and timing. Each summary names its scan. `docs/sources.md` lists the sources.
- Use period manuals and first-hand measurements only. Do not learn how the hardware works from
  the source code of another emulator. If a fact is missing, ask. Do not guess.
- A new board needs its entry in `reference/` and its row in `docs/sources.md`.

## License

Your contribution is under the MIT License of the project (`LICENSE`).
