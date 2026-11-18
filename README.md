# pascal100

A C++ simulator of the **Digicomp Research Pascal-100** and the IEEE 696 (S-100) boards around it.

The Pascal-100 is one S-100 board set with two processors: a Zilog Z80 and a Western Digital
**Pascal MicroEngine** (WD9000). The MicroEngine runs UCSD Pascal III.0 P-code in hardware. Only one
processor is active at a time, and the guest switches between them.

**Status:** version 0.1.0, the first release. The Pascal-100 machine (`examples/pascal/pascal.toml`)
boots a UCSD Pascal III.0 system disk on the MicroEngine. The CompuPro machines run CP/M 2.2 on the
Z80, and CP/M-86, CP/M 8-16 and MP/M 8-16 on the 8088 of the CompuPro CPU 8085/88. We test only
macOS by hand (see [Platforms](#platforms)). The board document is
`docs/boards/digicomp-pascal100.md`.

## What it simulates

The bus is IEEE 696: 24 address lines, 16-bit I/O addresses and the 16-bit transfer. It is the only
bus in pascal100.

| Group | Boards |
|---|---|
| Processors | `pascal100` (Z80 and MicroEngine), `cpu8588` (CompuPro CPU 8085/88: 8085 and 8088), `z80`, `8085` |
| Disk controllers | `disk1` (CompuPro Disk 1), `disk1a` (CompuPro Disk 1A), `disk2` (CompuPro Disk 2 hard disk) with `selch` (Selector Channel), `tarbell`, `tarbelldd`, `jadedd` |
| Serial and parallel | `interfacer1`, `interfacer3`, `interfacer4` (CompuPro Interfacers), `2sio`, `gsio`, `io4`, `c700` (Centronics printer) |
| System support and clock | `ss1` (CompuPro System Support 1: interrupts, timer, clock, serial), `rtc100` (clock) |
| Memory | `memory`, `bankmem` |
| Other | `pb1` (EPROM programmer), `hostbridge` (file transfer to the host) |

The `hostbridge` board is not a period board. The CP/M tools R.COM, W.COM and HDIR.COM use it to copy
files between the host and a CP/M disk. pascal100 keeps the same board identity as altairsim, so
these tools work unchanged.

Type `SHOW BOARDS` at the prompt for the full list, and `SHOW BOARD <type>` for the settings of one board.

## Machines

A machine is a TOML file. Each file in `examples/` is a complete machine: every board is in
the file, so you can read how the machine is built from top to bottom. Copy one and change it to
make your own.

| File | What it is |
|---|---|
| `examples/cpm22/cpm22-buffered.toml` | CompuPro CP/M 2.2K: Z80, Interfacer 1, Disk 1, host bridge, 64K. |
| `examples/cpm22/cpm22-terminal.toml` | The same machine, with the console in a VT100 window. |
| `examples/cpm86/cpm86.toml` | CompuPro CP/M-86 1.1 on the CPU 8085/88, in a window. |
| `examples/cpm8-16/cpm816.toml` | CompuPro CP/M 8-16 1.1R: 8086 programs on the 8088, 8080 programs on the 8085. |
| `examples/mpm8-16/mpm816.toml` | CompuPro MP/M 8-16 2.1F from a Fujitsu M20 hard disk: three users in three windows. |
| `examples/pascal/pascal.toml` | The Pascal-100 system: the Pascal-100 card, Disk 1, Interfacer 1, 128K, in a VT52 window. |
| `examples/debugger/debugger.toml` | A small program and its symbols, to learn the monitor's debugger. |
| `examples/ai-mcp/cpm-ai.toml` | A CP/M machine for an AI assistant that drives pascal100 over MCP. |

Each machine that opens a window has a `-console` twin with the console on your terminal, for
example `examples/pascal/pascal-console.toml`. `examples/README.md` describes each example.

Each example has a disk beside it that is ready to boot. The Pascal-100 distribution disks need a
build step before they boot (Pascal-100 manual, chapter V D).

One machine is built into the binary: `default`, the CompuPro workbench above with no disk. You
get it when you start `pascal100` with no machine.

## Start a machine

1. Start the program with a machine file. The examples boot at once:
   ```
   cd examples/pascal
   pascal100 pascal.toml
   ```
2. Or start the built-in machine, put your disk in drive 0, and start it:
   ```
   pascal100
   pascal100> MOUNT fd0:drive0 "CPM22N.DSK"
   pascal100> RUN 0
   ```

The prompt `pascal100>` is the monitor. Type `HELP` for the commands.

## Build

You need a C++20 compiler and CMake 3.20 or later. No other package is necessary.

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build -LE slow
```

Two parts are optional. CMake finds them if they are installed:

- **SDL3** opens a window for the built-in `terminal:` endpoint (VT52, VT100, ADM-3A, H19). Without SDL3, the build
  is headless and every test still passes. Use `-DPASCAL100_ENABLE_SDL=OFF` to build headless when
  SDL3 is installed.
- **CUPS** (macOS and Linux) sends the `printer:` endpoint to a real printer.

The test `acceptance-pascal100-boot` boots a copy of `tests/media/pascal100/pascal-dd.dsk`. To boot
your own Pascal-100 system disk instead, set `PASCAL100_BOOT_DISK` to a copy of it.

## Platforms

**We test only macOS on Apple silicon by hand.** Windows 11 x64 and Linux x64 are targets. At
this time, we do not have a Windows 11 or Linux system to test on.

| Package | How it was tested |
|---|---|
| macOS, Apple silicon | Built and used on a Mac. Each change passes the test suite there before it is merged. |
| Windows 11 x64 | Built by GitHub Actions (MSVC). The fast tests pass there, and CP/M boots from the unpacked package. Not tried on a real Windows 11 system. |
| Linux x64 | Built by GitHub Actions (GCC). The fast tests pass there, and CP/M boots from the unpacked package. Not tried on a real Linux system. |

If you use pascal100 on Windows or Linux, please open an issue and say what works and what does not.

**Apple has not notarized the macOS program.** macOS stops a program that you download with a
browser if Apple has not notarized it. Remove the download mark from the folder that you unpacked:

```
xattr -dr com.apple.quarantine pascal100-0.1.0-macos-arm64
```

You can also do it in System Settings:

1. Start the program once. macOS stops it.
2. Open System Settings > Privacy & Security.
3. Click **Open Anyway**.

## Documentation

The documentation is being rewritten for pascal100. The quick start, the introduction, the
package chapter, the worked examples, the changelog, *The Monitor* and *The Debugger* are up to
date. Other chapters of the User Manual and the Developer Guide still describe parts of
altairsim and its Altair machines, and say so at the top. These files apply to pascal100:

- `docs/QUICK-START.md`: boot CP/M with one command.
- `docs/boards/digicomp-pascal100.md`: the Pascal-100 board.
- `docs/manual/ref/`: the boards, machines and commands, generated from the binary.
- `docs/howto/`: step-by-step guides, including how to build the Pascal-100 system from its
  distribution disks.
- `DESIGN.md`: the design of the simulator, with a heritage note at the top.

## Origin and license

pascal100 started on 2026-11-02 as a copy of **[altairsim](https://github.com/deltecent/altairsim)**
by Patrick Linstruth. altairsim simulates the MITS Altair 8800 and the S-100 bus. pascal100 is not
part of altairsim, and the author of altairsim does not support it.

pascal100 keeps the framework of altairsim and the boards that the Pascal-100 system needs. It
removes the Altair bus, the Altair machines and the boards that have no use here. It adds the
Pascal-100 card with its two processors, the MicroEngine P-machine and IEEE 696 extended
addressing. Fixes to boards and processor cores in altairsim come into pascal100 by cherry-pick.

In the files of release 0.1.0, an issue or pull request number such as `#543` refers to
[deltecent/altairsim](https://github.com/deltecent/altairsim/issues), not to this repository.

The 8088 of the CompuPro CPU 8085/88 uses the x86 instruction set of
**[Clock Signal](https://github.com/TomHarte/CLK)** (CLK) by Thomas Harte, under the MIT License.
The code is in `third_party/clk`, with its license and a list of our changes.

pascal100 uses the MIT License, the same license as altairsim. See `LICENSE` and `NOTICE`.

The MIT License does not apply to the disk images, ROM images and manuals in this repository. They
hold software and text by their original publishers, such as Digital Research, CompuPro and
the publishers of UCSD Pascal, and keep the terms of those publishers. pascal100 includes the disk images so that
each example boots at once.
