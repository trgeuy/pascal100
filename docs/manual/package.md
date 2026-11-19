# What is in the package

```
pascal100                the program. One file, nothing to install.
QUICK-START.pdf          boot CP/M with one command. Start here.
pascal100-manual.pdf     this manual.
pascal100-changelog.pdf  what changed in this release, and in the releases before it.
pascal100-cheatsheet.pdf every command and option, formatted for reading.
pascal100-monitor.pdf    the pascal100> prompt: how to control the machine from the console.
pascal100-debugger.pdf   breakpoints, stepping, and how to look at the bus.
DRIVING-WITH-AI.md       for an AI assistant that controls the machine. See below.
cheatsheet.md            the same reference as plain text, for an AI assistant to read.
LICENSE                  the MIT license of pascal100.
LICENSE-SDL3             the license of SDL3, which is built into the program.
LICENSE-CLK              the license of the 8088 code from Clock Signal, which is built into the program.
examples/                machines that boot, with their media.
hostbridge/              the file-transfer utilities: source, HEX and COM.
tools/                   scripts that show a mirror, and scripts that open LBR files.
skills/                  the AI briefing again, and the rules for guest programs, as Agent Skills.
```

That is the whole package. You do not install a library or a runtime. You do not have to write
a machine file before the program starts.

**Start with `QUICK-START.pdf`.** It boots CP/M with one command. It shows you the `Ctrl-E` key
and the `RUN` and `QUIT` commands, which move you between the running machine and the monitor.
That one page is enough to get a machine running. This manual gives the full detail.

`pascal100-changelog.pdf` is the release history. It tells you what each version added.

`pascal100-monitor.pdf` and `pascal100-debugger.pdf` are two more documents that ship beside
this manual. This manual describes the simulated hardware. *The Monitor* describes the
`pascal100>` prompt, where you start and stop the machine. *The Debugger* describes how to find
a fault from that prompt. Each document tells you at the top where to start.

`pascal100` is one program that needs no other files. It uses one outside library, **SDL3**,
to open the terminal window. SDL3 is built into the program, so its license is in the package
as `LICENSE-SDL3`. The 8088 uses the x86 code of **Clock Signal** (CLK) by Thomas Harte. That
code is also built into the program, so its license is in the package as `LICENSE-CLK`.

The **Developer Guide** is not in the package. It is with the source, at the address in
[What is not in the package: the source](#what-is-not-in-the-package-the-source). You need it
only if you want to build a board of your own.

### `DRIVING-WITH-AI.md`

This document is for an **AI assistant**, not for you. It tells the assistant how to control the
machine through the MCP interface of the program. To use it:

1. Make a working folder. If your assistant reads Agent Skills, install the skills as this
   section shows below. If it does not, put `DRIVING-WITH-AI.md` and the `skills` folder in the
   working folder. The document names files in `skills/`, which have the rules for programs such
   as `ASM` and `R`.
2. Start an assistant in that folder.
3. Ask for what you want, for example: *"Using pascal100, boot CP/M and show me what is on the
   disk."*

If you do not use an AI assistant, you can ignore this document. Nothing else needs it.

`skills/` contains **Agent Skills**. Each folder in it is one skill. `skills/pascal100/` is the
same briefing as `DRIVING-WITH-AI.md`. Each other folder, such as `skills/pascal100-hostbridge/`,
has the rules for one program that runs in the machine. Agent Skills come from Anthropic, for
its Claude assistants. Claude finds a skill from its description, so when you ask for a machine,
Claude reads the briefing itself. You do not have to tell it which file to open. To install the
skills for Claude Code, copy every folder in `skills/` to one of these places:

- `.claude/skills/` in your project folder, for that project only
- `~/.claude/skills/` in your home folder, for every project

The folders need no other files, so they work in either place. Some other assistants can also
read skills. Their documentation tells you where to put the folder. If your assistant does not
read skills, give it `DRIVING-WITH-AI.md` and the `skills` folder as the steps above show.

The quick reference lists every option, every monitor command, every board and every machine.
The program generated it, so it matches the program that you have. It ships in two forms with
the same content. `pascal100-cheatsheet.pdf` is for you. `cheatsheet.md` is plain text, for an
AI assistant to read.

## The machines are in the program

You do not need any files to run a machine. One machine is built into the program:

```
$ pascal100                       the default machine: a CompuPro workbench
$ pascal100 --list                the built-in machine, with a sentence about it
```

The built-in machine is an ordinary machine file that is stored inside the program. It uses the
same TOML format that you would write yourself. `CONFIG SAVE mine.toml` saves any running
machine to a file that you can edit. The other machines, such as the Digicomp Pascal-100, are
files in `examples/`.

**The built-in machine has only a boot EPROM, and it starts with empty drives.** It does not
run until you type `RUN 0`. Mount a disk first, which the next sections are about. The machines chapter gives the full detail.

## The examples, media included

`examples/` contains complete machines. **Each example is a folder with its media in it.** Every
example boots as soon as you unzip the package. You do not have to get or mount anything.

**Every folder has its own README**, in Markdown and as a PDF. The README describes the
example: what the machine is, what is in the drive, what to type, and what
the machine should print. This manual does not list the examples. Look in `examples/`, and read
the README of the example that you want.

```
$ ls examples/
$ pascal100 examples/cpm22/cpm22-console.toml
```

**You can move an example folder anywhere.** It still boots after you copy it, rename it or send
it to another person. The machines chapter tells you how the paths in a machine file work.

The examples chapter describes some of the examples in detail.

## The file-transfer utilities

`hostbridge/` contains `R`, `W` and `HDIR`. The file-transfer chapter uses these programs to
move files between CP/M and your computer. The folder has the 8080 **source**, and the assembled
`.HEX` and `.COM` files.

You do not need this folder to use the utilities on the CP/M disk in the package. That disk
already has the `.COM` files. The folder is for a disk that does not have the utilities. The
file-transfer chapter tells you how to put them on that disk, by pasting `R.HEX` through the
console.

## The watch scripts

`tools/` contains two scripts that show the console of the machine in a second window, while
an AI assistant controls the machine: `tools/mirror-watch.sh` for macOS and Linux, and
`tools/mirror-watch.ps1` for Windows. Each script waits for the mirror, shows its output, and
connects again after the simulator starts again. `tools/README.pdf` gives the commands. The
chapter *The MCP server* tells you how to start a mirror.

## The LBR tools

Much CP/M software is kept in library files (`.LBR`). The files in a library are often
crunched (a `Z` in the extension, such as `.AZM`) or squeezed (a `Q`, such as `.AQM`).
`tools/` has three Python 3 scripts for these files. `tools/lbr-extract.py` copies the files
out of a library. `tools/uncrunch.py` decodes a crunched file, and `tools/unsqueeze.py` decodes
a squeezed file. Each decoder gives the file its original name. You must install Python 3 to
use these three scripts. `tools/README.pdf` gives the commands.

## What is *not* in the package: everything else to run

**The media in `examples/` is all the media in the package.** The other built-in machines start
with an empty drive. `SHOW MOUNTS` at the monitor shows what is in each drive. You supply the
disk, and you `MOUNT` it. The disks chapter tells you how. When that chapter names an image that
is not in `examples/`, the name shows the form of the command. It is not a file that you have.

Most disk images are not ours to give away, so they are not in the package. `examples/pascal`
has one UCSD Pascal III.0 system disk for the Pascal-100. The MIT license of pascal100 does not
cover it.

## What is *not* in the package: the source

The **source code** of the simulator is not in the package. pascal100 is a private project
under the MIT license, maintained by Timothy Geuy. It comes from `altairsim`, Patrick
Linstruth's open simulator of the MITS Altair 8800.

The only source in the package is in `hostbridge/`. That is the 8080 source of the file-transfer
utilities, not the source of the simulator.

You do not need the source to use anything in this manual. If you want to **build a board of
your own**, you need the source and the *Developer Guide*. This manual tells you how to use the
machine. It does not tell you how to add to the program.

## Reporting a bug, or asking for something

Report a bug or ask for a feature to the person who gave you the package, or to the maintainer,
Timothy Geuy. Give the exact command, the machine file and what the program printed.

pascal100 is not part of altairsim. Do not report a pascal100 problem to the altairsim project.
