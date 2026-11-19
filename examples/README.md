# examples

**Machines that boot.** Each directory here is self-contained: a `.toml` that describes the
machine, the media it needs lying beside it, and a note saying what you will see. Copy any one of
them anywhere and it still runs — a path inside a machine file resolves against **that file**, not
against the directory you launched from.

## In the release

These eight examples come in the release package. Each folder holds a machine file, the media
that it needs and a README that tells you what to type.

```
pascal100 examples/cpm22/cpm22.toml           # CompuPro CP/M 2.2K, in a VT100 window
pascal100 examples/cpm86/cpm86.toml           # CompuPro CP/M-86 1.1 on the CPU 86/87, in a window  
pascal100 examples/cpm8-16/cpm816.toml        # CompuPro CP/M 8-16 1.1PD from a Disk 3 hard disk
pascal100 examples/mpm8-16/mpm816.toml        # CompuPro MP/M 8-16 2.1F from a hard disk, 3 users
pascal100 examples/pascal/pascal.toml         # UCSD Pascal III.0 on the Pascal-100, in a VT52 window
pascal100 examples/debugger/debugger.toml     # a bench for learning the symbolic debugger
```

| | What it is |
|---|---|
| [`cpm22/`](cpm22/) | CompuPro **CP/M 2.2K**, 64K, booted by the Disk 1 boot EPROM from an 8" floppy. `A>` in one command, in a **built-in VT100 window** the simulator draws itself — no telnet client, no external emulator. `cpm22-console.toml` puts the console on stdio. |
| [`cpm86/`](cpm86/) | CompuPro **CP/M-86 1.1** on the CPU 86/87: the 8086 boots and runs CP/M-86. 8086 programs (`.CMD`) only. `cpm86-console.toml` puts the console on stdio. |
| [`cpm8-16/`](cpm8-16/) | CompuPro **CP/M 8-16 1.1PD** from a 40 MB Quantum Q540 hard disk on a **Disk 3**: CP/M-86 plus CompuPro's `SW!`, which runs 8080 programs (`.COM`) on the 8085 beside the 8086 ones. The floppy boots `CPM340.SYS`, and `A:` to `E:` are the hard disk. `cpm816-console.toml` puts the console on stdio. |
| [`mpm8-16/`](mpm8-16/) | CompuPro **MP/M 8-16 2.1F**, multi-user, from a Fujitsu M20 hard disk on a Disk 2 and Selector Channel: the console, tty1 and tty2 in three windows, two printers to files. Type `SYNC` before you stop. `mpm816-console.toml` puts the console on stdio. |
| [`pascal/`](pascal/) | **UCSD Pascal III.0** on the Digicomp Pascal-100 (Z80 and MicroEngine), booted from an 8" floppy in a **built-in VT52 window**. `pascal-console.toml` puts the console on stdio instead. The disk is not covered by the MIT license. |
| [`debugger/`](debugger/) | A 46-byte program with its **symbols** and a guided walk through the monitor's debugger: `SYMBOLS LOAD`, symbolic `DISASM`, single-step, break on a label, run. |
| [`ai-mcp/`](ai-mcp/) | A working directory for an **AI assistant driving pascal100 over MCP**: a CP/M machine and a tiny `HELLO.ASM` with one deliberate bug the assistant assembles, runs, single-steps to find, and fixes — all through the simulator's MCP tools. See `DRIVING-WITH-AI.md`, or the `pascal100` Agent Skill, which an assistant that reads skills loads by itself: `skills/pascal100/` in the package, `.claude/skills/pascal100/` in this repository. |
