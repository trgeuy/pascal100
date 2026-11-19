---
name: pascal100-cpm-build
description: How to build a CP/M program inside a pascal100 machine — assemble 8080 source with Digital Research `ASM` and `LOAD`, or with the macro assembler `MAC` (and `Z80.LIB` for Z80 instructions), run it, and bring the `.COM`, `.HEX` and `.PRN` back to the host. Use this whenever a task assembles, builds or rebuilds a CP/M program or an 8080 `.ASM` file, when `ASM` prints an error letter or the program has a wrong byte, and before you look for a cross-assembler on the host.
---

# Build a CP/M program in the machine

This skill needs the `pascal100` skill, which tells you how to boot a machine and type at it.
Each step here was replayed on `examples/cpm22/cpm22-console.toml` (2026-11-09). The disk of that
machine has `ASM`, `MAC` with `Z80.LIB`, `LOAD`, `DDT` and the host bridge utilities.

Build in the machine. The assembler that the program was written for is on the disk, and no
host tool is necessary.

Two other skills hold rules that this one uses:

- `pascal100-cpm-text`: the source file must have CR/LF line ends.
- `pascal100-hostbridge`: `R` and `W`, which move the files.

## The steps, with `ASM` and `LOAD`

Start `pascal100` in the folder that has the source, so that `R` finds it.

```
run {from: 0, until: "A>"}                                  # boot CP/M (the Disk 1 boot EPROM at 0)
run {input: "R FOO.ASM\r", until: "\nA>"}                   # host -> CP/M
run {input: "ASM FOO\r", until: "\nA>", timeout_ms: 120000} # makes FOO.HEX and FOO.PRN
run {input: "LOAD FOO\r", until: "\nA>"}                    # makes FOO.COM
run {input: "FOO\r", until: "\nA>"}                         # run it
run {input: "W FOO.COM\r", until: "\nA>"}                   # CP/M -> host, binary
run {input: "W FOO.PRN\r", until: "\nA>"}                   # the listing, as text
```

Use the prompt that your BIOS prints. To test against a real serial device after the build, set
`clock_hz` on the running machine; the recipe is in "Driving a real serial device" in
`DRIVING-WITH-AI.md`.

A correct assembly prints the next free address, a `USE FACTOR` line and `END OF ASSEMBLY`,
and no other line:

```
CP/M ASSEMBLER - VER 2.0
010C
000H USE FACTOR
END OF ASSEMBLY
```

`LOAD` prints `FIRST ADDRESS 0100` for a program that starts at `ORG 100H`.

## Read what `ASM` prints

**`ASM` shows an error as one letter before the source line. It still writes the `.HEX`
file.** A build that has an error letter makes a program with a wrong byte in it. Read the
output of each assembly. If a line starts with a letter, correct the source.

```
S               MY_VAL  EQU     42H
E0100 3E00              MVI     A,MY_VAL
```

The rules of `ASM` that cause these errors:

- **A symbol cannot have an underscore.** `MY_VAL EQU 42H`
  gets an `S` error. Each line that uses the symbol gets an `E` error and assembles `00`.
- **An instruction name cannot be a symbol.** `JMP EQU 0C3H` gets an `E` error, and `ASM`
  assembles the line as a `JMP` instruction.
- **Upper case and lower case are the same.** A label `fujiRd` and `FUJIRD EQU 52H` are one
  name. The `EQU` gets a `P` error, the label gets a `V` error, and `MVI A,FUJIRD` assembles
  `00`.
- **The line after the banner is `0000`, or correct lines get errors.** The source file has
  LF line ends. See the `pascal100-cpm-text` skill.

When a constant is important, read the `.PRN` file and check the bytes. A `00` where a value
must be shows that a symbol was not defined.

## The steps, with `MAC`

`MAC` is the Digital Research macro assembler. It takes the same 8080 source as `ASM`, plus
macros. With `MACLIB Z80` at the top of the source, `Z80.LIB` gives macros for Z80 instructions
in 8080 style, for example `DJNZ LOOP`.

```
run {input: "R FOO.ASM\r", until: "\nA>"}
run {input: "MAC FOO\r", until: "\nA>", timeout_ms: 120000}   # makes FOO.HEX, FOO.PRN, FOO.SYM
run {input: "LOAD FOO\r", until: "\nA>"}                      # makes FOO.COM
```

A correct assembly prints `CP/M MACRO ASSEM 2.0`, the next free address, a `USE FACTOR` line
and `END OF ASSEMBLY`. `Z80.LIB` must be on the same disk as the source.

## Disk space

The disk in `examples/cpm22` has about 488K free (`STAT`). One build makes two or three files
beside the source. When the disk is full, `ASM` prints `OUTPUT FILE WRITE ERROR`, and the
`.HEX` file is empty or not complete.

- Look at the space: `run {input: "STAT\r", until: "\nA>"}`.
- Erase the files of a build after you copy them out: `ERA FOO.HEX`, `ERA FOO.PRN`.
