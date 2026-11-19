# Working on `pascal100`

A C++20 simulator of the **Digicomp Research Pascal-100** (a Z80 and a Western Digital MicroEngine
on one S-100 board set) and the IEEE 696 boards around it. No dependencies beyond a C++20 compiler
and CMake; SDL3 is optional and detected, never required. The project began as a copy of Patrick
Linstruth's `altairsim` (see `NOTICE` and the heritage note in `DESIGN.md`). The maintainer is
`trgeuy` (Timothy Geuy).

## How work is done here

1. **Agree on the change first.** Say what you will change and why, and wait for the maintainer.
   For a feature, first show that it is needed: not already there, not a gap in the docs.
2. **Tests never work around a bug.** A test that exposes a bug gets the code fixed. Never loosen
   an assertion, skip the test, dodge the input or add a retry.
3. **A bug in the area of the task is fixed in the same change.** An unrelated bug is reported to
   the maintainer and left alone.
4. **Docs change with the code**, in the same commit.
5. **No commit and no push without the maintainer's approval.** Report what changed and the test
   results, then stop.
6. **Work goes on a topic branch** (one per change, for example `disk3`), and reaches `master`
   through a pull request. CI starts by hand only: `gh workflow run ci.yml --ref <branch>`. Do
   not run CI on a commit that already passed, and do not run it for a docs-only change.
7. **A fix from altairsim comes in by cherry-pick** (`git cherry-pick -x`). The C++ namespace
   `altair` and the internal `altair_*` targets keep their names so that cherry-picks apply.

## Before changing anything

| | |
|---|---|
| [`DESIGN.md`](DESIGN.md) | The architecture and the reasoning. Read the relevant section first. Most of it is altairsim's text; amendments are marked **pascal100:**. |
| `docs/boards/` | One document per board. `digicomp-pascal100.md` is the Pascal-100. |
| `reference/` | Text summaries of the period manuals each board is modeled from. |
| `docs/devguide/` | The Developer Guide (altairsim's, not yet rewritten). |
| [`DISTRIBUTION.md`](DISTRIBUTION.md) | altairsim's release process, kept for the release step. Not adapted yet. |

## Rules that bite

- **The word is "board", not "card"** (`DESIGN.md` 0.3), for the object, the command and the
  table. *Card* only where the sentence is about the physical artifact.
- **`docs/manual/ref/` is generated from the binary.** Change the generator and run
  `cmake --build build --target docs-reference`. Never edit those files by hand.
- **The manual may not name anything outside the package**: no source paths, no `CMakeLists`,
  no `ctest`, no `DESIGN.md`. `tests/acceptance/docs-manual.cmake` checks this.
- **A new manual chapter goes into `docs/manual/ORDER`**, or it is not in the PDF.
- **The PDFs come from `.github/workflows/docs.yml`** (pandoc 3.6), which runs only when started
  by hand. If you run `tools/build-docs.sh` locally, `git checkout --` the PDFs afterwards.
- **Never give hardware a behavior it never had** to fix a software symptom. Look at the host,
  filter and monitor layers first.
- **Third-party software stays out of new disk images.** Period DRI and CompuPro distribution
  files are fine; Microsoft, Sorcim (SuperCalc) and similar programs are not.
- **Compatibility names stay:** the host bridge ident `ALTAIRSIM HOSTBRIDGE 1` (R, W and HDIR on
  existing CP/M disks check it) and the snapshot magic `ALTRSNP1`.
- **`<!-- @claude ... -->` in a Markdown file is a review comment for you.** See
  `docs/devguide/doc-review-comments.md`.

## Testing

```sh
./build/altair_tests <names>        # the suites for the subsystem you touched
./build/altair_tests --list         # the suite names
ctest --test-dir build -LE slow -j 4   # all tests except the slow CPU exercisers
ctest --test-dir build -j 4            # all tests
```

Read the pass line, `100% tests passed out of N`. A stray `cd` can stop `ctest` from running,
and that looks like success.

`acceptance-pascal100-boot` needs a Pascal-100 system disk, which is not in the repository. Set
`PASCAL100_BOOT_DISK` to a **copy** of the disk.

Warnings fail CI on GCC and MSVC. Check before a code commit:
`cmake -B build -DWERROR=on && cmake --build build -j`.

## Driving a running guest

**To type at a running guest and read what it prints, use `pascal100 <machine> --mcp`, and load
the `pascal100` skill for the recipe.** Never write a throwaway `expect` or pty script. `expect`
under `tests/acceptance/*.exp` is only for a committed acceptance test.
