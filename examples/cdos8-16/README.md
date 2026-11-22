# CompuPro Concurrent DOS 8-16 4.1 from a hard disk

**To start it**, name the machine file from inside this folder:

```
$ pascal100 cdos816.toml
```

On **Windows** the program is `pascal100.exe`. The machine boots by itself. Four windows open:

| Window | What it is |
|---|---|
| **Console 0 -- PC Video** | The PC Video board: an IBM PC style screen and keyboard. It shows the sign-on and the `0A>` prompt. |
| **tty1 -- console 1** | A VT100 terminal on an Interfacer 4 serial port. It shows the sign-on of the shell and its own `0A>` prompt. |
| **tty2 -- console 2** | The same, on a second Interfacer 4. |
| **tty3 -- console 3** | The same. |

Console 0 shows this:

```
CompuPro XIOS Ver 4.1D
...
    CompuPro CDOS V4.1 (Wotan's Eye)
Concurrent DOS 8-16 Copyright (C) 1984, 1985 VIASYN
...
A>SPZ
Detaching from console. SPUZ is ready to run 8 bit tasks
A>PRINTMGR START 0 1 2
...
CompuPro Concurrent DOS 8-16
PC Mode
0A>
```

The `0` in `0A>` is the user number. "PC Mode" is the name of the command shell. It shows on
every console.

`cdos816-mono.toml` is the same machine with a monochrome PC Video
([Monochrome](#monochrome)). `cdos816-console.toml` puts tty1 on the terminal where you started
the program, and needs no window ([The files](#the-files)).

`^E` (STOP) gives the keyboard back to the monitor at any time. `RUN` continues the guest.

## What this system is

Concurrent DOS 8-16 is CompuPro's version of Digital Research's Concurrent DOS 4.1 (1985), for
the CompuPro CPU 286 card. It is a multi-user and multi-tasking system. Each console has its own
user and its own programs, and all of them run at the same time. It runs three kinds of program:

| Program | File | Runs on |
|---|---|---|
| A CP/M-86 program | `.CMD` | The 80286 |
| A PC-DOS program | `.EXE`, or `.COM` with no attributes | The 80286, in PC Mode |
| A CP/M-80 (8080 or Z80) program | `.COM` with the attributes `F1`-`F4` set | The Z80 of the SPUZ board |

`SPZ` starts the SPUZ manager. `AUTO.BAT` on `A:` runs it at each boot, so you do not type it.

This example is not CompuPro's distribution disk. It is a working system, made from the 4.1
files, with the consoles, the printers and the tools set up. [How the disks were
made](#how-the-disks-were-made) tells what is on it.

## The consoles

**Console 0** is the PC Video. It has four virtual consoles, each with its own shell and its own
screen. To change the virtual console, hold Ctrl and press a key of the keypad:

| Virtual console | Ctrl + keypad | Or, in the pascal100 window, Ctrl + |
|---|---|---|
| 0 | `0` | Insert |
| 1 | `1` | End |
| 2 | `2` | Down arrow |
| 3 | `3` | Page Down |

**macOS:** Ctrl + Down arrow is the Mission Control shortcut "Application windows". It does not
get to the window, so you cannot select virtual console 2 with it. Use Ctrl + keypad `2`, or turn
the shortcut off: System Settings > Keyboard > Keyboard Shortcuts > Mission Control.

Row 25 of console 0 is the status line. It marks the front virtual console (`----` in its
place, `____` in the others), and shows the printer of the console and the time.

**`WINDOW`** shows two or more virtual consoles on the screen at the same time. For example,
virtual console 0 on rows 1-12 and virtual console 1 on rows 13-24:

```
0A>WINDOW CHANGE N=1 NR=12
0A>WINDOW CHANGE N=2 PR=13,NR=12
```

Window numbers are 1-4 for virtual consoles 0-3. The layout is lost when you restart.

**Consoles 1, 2 and 3** are the tty windows. Each is one console with its own shell.

## The drives

The Quantum Q540 hard disk (40 MB) is five drives. They have different sizes, from the disk
table of the XIOS (`DPBS.TBL` on `D:`):

| Drive | Size | What is on it |
|---|---|---|
| `A:` | 5 MB | The system, every command, `HELP` |
| `B:` | 10 MB | The work drive, with the demos |
| `C:` | 10 MB | Empty |
| `D:` | 10 MB | The XIOS 4.1D build kit ([Rebuild the system](#rebuild-the-system)) |
| `E:` | 1.2 MB | Empty. The size of an 8-inch double-sided floppy |

The Disk 1A floppy controller has four drives:

| Drive | Floppy | In this machine |
|---|---|---|
| `I:`, `J:` | 8-inch, drives 0 and 1 | Empty |
| `K:`, `L:` | 5.25-inch, drives 2 and 3 | Empty |

All four floppy drives are empty. [A blank floppy](#a-blank-floppy) tells how to put a disk in one.

**`N:`, `O:` and `P:` are the root of `A:`.** They are not disks: the XIOS has no drive for
them. Concurrent DOS gives these letters to the directory of a real drive, and here that is
`A:\`. `DIR P:` shows the files of `A:`, and `CHDIR N:` shows `A:\`. The shell uses `P:` for
its `COMSPEC`. The drives `A:`-`E:` have no subdirectories ("Requires DOS media" from `MKDIR`),
so the three letters always show the root of `A:`.

**`STAT` with no drive shows only the drives in use.** A drive comes into use when a command
first reads it. At the start, these are `A:`, `N:`, `O:` and `P:`. After `DIR B:`, `B:` is in the
list too. `STAT E:` shows one drive.

**A command must be on the current drive, or on `A:` with the SYS attribute.** The shell looks
in no other place. All commands are on `A:` with SYS. Your own programs go on the drive where you
use them.

## A tour

Type these at console 0. Each step tells you what to look for.

1. **Look at the system.**

   ```
   0A>DIR
   0A>SDIR
   0A>SHOW
   ```

   `DIR` shows the files and "System files exist". `SDIR` shows all the files, with their
   attributes. `SHOW` shows the free space of the drive.

2. **Get help.** `HELP` shows the list of topics, then the `HELP>` prompt. Type one topic name
   at the prompt, for example `ERA`. Press Enter at an empty `HELP>` prompt to go back to the
   shell. See [HELP](#help).

3. **Run the two demo programs** on `B:`:

   ```
   0A>B:
   0B>HELLO80
   Hello from ASM and LOAD on the SPUZ
   0B>HELLO86
   Hello from the 8086 tools on the 80286
   ```

   `HELLO80.COM` is an 8080 program. It runs on the Z80 of the SPUZ. `HELLO86.CMD` is a CP/M-86
   program. It runs on the 80286.

4. **Assemble the 8080 program again.**

   ```
   0B>ERA HELLO80.COM
   0B>ASM HELLO80
   0B>LOAD HELLO80
   0B>SUBMIT A:8BIT HELLO80
   0B>HELLO80
   ```

   `ASM` and `LOAD` are Digital Research's 8080 tools from CP/M 2.2. They are 8080 programs
   too, and run on the SPUZ. `8BIT.SUB` sets the attributes `F1`-`F4` on the new `.COM`. Without
   them, the shell runs a `.COM` as a PC-DOS program on the 80286.

5. **Assemble the 8086 program again.**

   ```
   0B>ERA HELLO86.CMD
   0B>RASM86 HELLO86
   0B>LINK86 HELLO86
   0B>HELLO86
   ```

   `ASM86` and `GENCMD` make the same program the older way.

6. **Edit a file.** On console 0, use `ED`, Digital Research's line editor:

   ```
   0B>ED HELLO80.ASM
   ```

   In a tty window, use `VI`, a full-screen editor in the style of vi. It is
   [vi-cpm22](https://github.com/deltecent/vi-cpm22) V1.4, by Patrick Linstruth (deltecent).
   `VI.DOC` on `A:` tells how to use it:

   ```
   0A>VI B:HELLO80.ASM
   ```

   `VI` needs a VT100 terminal, so it runs only in the tty windows, not on console 0. Type
   `:wq` and Enter to write the file and stop.

7. **Draw on the PC Video.** `GFX` uses the PC BIOS graphics calls (`INT 10h`):

   ```
   0B>GFX
   ```

   It draws a frame, a diagonal and a square in 320 x 200 colour. Press a key. It draws a line and a
   diagonal in 640 x 200. Press a key. It goes back to text and prints two lines:

   ```
   M4 MODE=04 DOTS=03 03 03 03 81
   M6 MODE=06 DOTS=01 01 01
   ```

   `MODE` is the mode that the BIOS gives back: correct. The `DOTS` values are not the colours
   that were drawn. They come from function 13 (read a dot), and that function gives back
   nothing in this system ([Known limits](#known-limits)). `GFX.S` is the source, for the GNU
   assembler on the host. You cannot change virtual console in graphics mode.

8. **Print a file.** See [Printers](#printers).

   ```
   0A>PIP LST:=LPRS
   ```

   The file comes out in `print0.txt` in this folder.

9. **Copy a file to the host and back.** `W` writes a file to the folder you started pascal100
   in. `R` reads one from it, to the current drive:

   ```
   0A>W TTYS
   0A>B:
   0B>A:R TTYS
   ```

   A wildcard in `W` can match 64 files at most. For more, give more than one wildcard, for
   example `W A*.*`, then `W B*.*`. See
   [Moving files in and out](../../docs/manual/file-transfer.md).

10. **Format a floppy.** Put a blank disk in `K:` and format it, as in
    [A blank floppy](#a-blank-floppy).

11. **Use two consoles at once.** Type `DIR` in the tty1 window while console
    0 runs `HELP`. Each console has its own shell. The consoles share the drives and the
    printers.

## A blank floppy

The floppy drives are empty when the machine starts. To put a new blank disk in `K:`, the first
5.25-inch drive (for an 8-inch disk, see the end of this section):

1. Press `^E` in the window where you started pascal100. The `pascal100>` prompt comes back, and
   the guest stops.
2. Mount a new disk image. `CREATE` makes the file in this folder, and
   `media=5ds` makes it a 5.25-inch double-sided disk:

   ```
   pascal100> MOUNT fd0:drive2 floppy-k.dsk CREATE media=5ds
   pascal100> RUN
   ```

3. Format it, on console 0:

   ```
   0A>FORMAT
   ```

   Answer `K` for the drive, `3` for the format and `Y` to start. At the end, FORMAT asks:

   ```
   Do you want to format another disk?
       R  -  request new parameters.
       cr -  duplicate last FORMAT.
       N  -  exit back to system.     >
   ```

   Answer `N`. `DIR K:` now shows "File not found", and the disk holds 772K.

Before the disk is formatted, `K:` gives "Invalid Drive". The disk keeps its files in
`floppy-k.dsk`. The machine file does not mount it, so after a restart, mount it again, without
`CREATE`:

```
pascal100> MOUNT fd0:drive2 floppy-k.dsk media=5ds
```

`fd0:drive3` is `L:`. For an 8-inch double-sided disk, use `fd0:drive0` (`I:`) or `fd0:drive1`
(`J:`) and `media=8ds`, and answer `I` or `J` to FORMAT:

```
pascal100> MOUNT fd0:drive0 floppy-i.dsk CREATE media=8ds
```
See [Disks](../../docs/manual/disks.md) in the pascal100 manual for `MOUNT`.

## HELP

`HELP.CMD` and `HELP.HLP` are on `A:`. `HELP.HLP` is the help text, and it must be on the current
drive: type `HELP` from `A:`. The program and its text are from Concurrent CP/M-86 2.0 (1983), so some topics differ from 4.1. It has 30 topics. For a name that is
not a topic, `HELP` gives "Topic: ... Not found". At the `HELP>` prompt, type only the topic name,
not `HELP` again.

## Printers

The system has three printers. Each one writes a file in this folder:

| Printer | Board | File |
|---|---|---|
| 0 | Interfacer 4, the Centronics port | `print0.txt` |
| 1 | Interfacer 4, serial port 5 | `print1.txt` |
| 2 | Interfacer 3, serial port 0 | `print2.txt` |

`PRINTER` shows the printer of your console and the program that has it. `PRINTER 1` selects
printer 1. A printer has one owner at a time. When console 0
prints, `PIP LST:` on console 1 gets "ERROR: PRINTER BUSY".

**The print manager** puts print jobs in a queue, so that two consoles can print at the same
time. `AUTO.BAT` starts it for the three printers (`PRINTMGR START 0 1 2`). Give it a job with
`PAGESIZE=64` or less:

```
0A>PRINTMGR PRINT TTYS [MARGIN=0, NOFEED, PAGESIZE=64, PRINTER=0]
0A>PRINTMGR STATUS
```

With the default page settings, the print manager refuses the job: "Formsize must be at least 2
lines greater than Pagesize". `PIP LST:` does not use the queue.

## Monochrome

```
$ pascal100 cdos816-mono.toml
```

The PC Video is set for a monochrome monitor: green text, characters of 9 x 14 dots, at the IBM
monochrome addresses (screen RAM at `0B0000`, ports `03B0`). A colour system writes to the colour
addresses, so the monochrome machine has its own disk, `cdos816-mono.dsk`. It is the same disk
with two files changed:

| File | On `cdos816.dsk` | On `cdos816-mono.dsk` |
|---|---|---|
| `CCPM.SYS` | The colour system | The monochrome system |
| The other system | `CCPMMONO.SYS` | `CCPMCOL.SYS` |

The loader reads `CCPM.SYS`, so the system with that name starts. The two disks are separate:
a file that you write on one is not on the other.

## Rebuild the system

`D:` holds CompuPro's XIOS 4.1D source (the XIOS is the part of the system that controls the
boards), Digital Research's system modules (`.CON`) and `GENCCPM`. `CCPM.SYS` on `A:` was built
from this kit. `XIOS.CON` on `D:` is the linked XIOS of that system. On `cdos816-mono.dsk`, it is
the monochrome XIOS.

**To change only a `GENCCPM` setting**, for example a line of `SYSPARAMS` in `XIOS.IN`, do steps 3
to 5. `GENCCPM` uses the `XIOS.CON` that is on `D:`.

**To change the XIOS source**, do all the steps. Step 1 writes a new `XIOS.CON`.

To build it again:

1. Go to `D:` and assemble and link the XIOS. This takes a long time: 27 assemblies, then the link.

   ```
   0A>D:
   0D>SUBMIT XIOS
   ```

2. Look for an empty object file. `SDIR *.OBJ` must show 27 files, none with 0 records. Once,
   two of them came out empty, and `LINK86` stopped with "DISK READ ERROR". Assemble an empty one
   again by hand, then link:

   ```
   0D>RASM86 PIC $PZ SZ
   0D>LINK86 XIOS[I
   ```

3. Make the system:

   ```
   0D>GENCCPM <XIOS.IN
   ```

   At the "Changes?" prompt, type `gensys` and Enter. `GENCCPM` writes `CCPM.SYS` on `D:`.

4. Copy it to `A:`. Keep the old system under another name, so that you can go back:

   ```
   0D>A:
   0A>REN CCPM.SYS CCPMOLD.SYS
   0A>PIP A:CCPM.SYS=D:CCPM.SYS[V]
   ```

5. Restart the machine.

**Keep a copy of `cdos816.dsk` before you change the system.** If the new system does not start,
use the copy.

`GLOBAL.EQU` sets the colour PC Video. For the monochrome system, do this before step 1:

```
0D>REN GLOBAL.EQU GLOBALCO.EQU
0D>PIP GLOBAL.EQU=GLOBALMO.EQU
```

 `HEADENTR.A86` holds the console and printer tables. `SLAVE.RSO` is
CompuPro's `SLAVE.RSP`, renamed so that `GENCCPM` does not put it in the system: with it, the
second `SPZ` stops every console.

## The machine

A CompuPro CPU 286 (80286), a System Support 1 (interrupt controllers and clock), a Disk 1A
floppy controller at `C0`, a Disk 3 hard disk controller with a Quantum Q540, two Interfacer 4s
(users 0-3 and 4-7) and an Interfacer 3 (users 8-15) for the consoles and the printers, a SPUZ
(Z80 with 256K), a PC Video, the host bridge at `B0`, and 944K of RAM. The RAM has two holes:
the screen RAM of the PC Video, and `0E0000`-`0EFFFF`, where the SPUZ shows its own memory.

**How it boots.** The Disk 1A boot ROM (routine 5) reads the loader from the hard disk. The
loader reads `CCPM.SYS` from `A:` and starts the system. `AUTO.BAT` then runs `SPZ` and the print
manager.

**Consoles and printers.** The XIOS tables (`HEADENTR.A86`) connect each console and printer to a
serial port. The machine file has the boards where the tables expect them:

| XIOS device | Board, port |
|---|---|
| Console 1 | Interfacer 4 (users 4-7), user 7 |
| Console 2 | Interfacer 4 (users 0-3), user 3 |
| Console 3 | Interfacer 4 (users 0-3), user 2 |
| Printer 0 | Interfacer 4 (users 4-7), Centronics |
| Printer 1 | Interfacer 4 (users 4-7), user 5 |
| Printer 2 | Interfacer 3, user 8 |

`TTYS` and `LPRS` on `A:` describe the same ports. `TTYS` also lists consoles 4-6, which this
system does not have.

## The files

| File | What it is |
|---|---|
| `cdos816.toml` | The whole machine: console 0 and the three ttys in windows. Needs a build with SDL3. |
| `cdos816-mono.toml` | The same machine with a monochrome PC Video and `cdos816-mono.dsk`. Needs a build with SDL3. |
| `cdos816-console.toml` | The same machine and disk, with tty1 on the terminal where you started the program, and tty2 and tty3 not connected. Wait a few seconds after the start, then press Enter for `0A>`. On a build with SDL3 the PC Video window opens too. |
| `cdos816.dsk` | The hard disk, colour system: a Quantum Q540, 512 cylinders, 8 heads, 9 sectors of 1024 bytes (37,748,736 bytes). |
| `cdos816-mono.dsk` | The hard disk, monochrome system. |

The program makes these files in this folder when the machine starts: `print0.txt`,
`print1.txt` and `print2.txt` (the printers). A floppy that you make with `MOUNT ... CREATE` is
there too.

## How the disks were made

1. CompuPro's `DISK3` formatted the Q540 (`DISK3 FORMAT Q540`). This was version 3.4, from
   Concurrent DOS 3.1. Version 4.6, on `A:`, needs a `BADSEC` file of the drive's bad sectors.
2. CompuPro's `SYSGEN` wrote the PC Video loader `LCCPMPC3.86` to the boot tracks.
3. `R` copied each file from the host. `SET` gave the commands on `A:` the attributes SYS and RO,
   and the 8080 programs the attributes `F1`-`F4`.
4. `CCPM.SYS` was built from the kit on `D:`, as in [Rebuild the system](#rebuild-the-system), then
   the disk was made again from step 1 with that `CCPM.SYS`.
5. For `cdos816-mono.dsk`, `REN` changed the names of the two system files.
6. `SUBMIT XIOS` built `XIOS.CON` for each disk, from the colour or the monochrome `GLOBAL.EQU`,
   and `R` copied it to `D:`. `GENCCPM` with it makes the `CCPM.SYS` of that disk.

What is on the disks:

- **`A:`.** The Concurrent DOS 4.1 system and commands, from CompuPro's 4.1 disks. Digital
  Research's 8080 tools from CP/M 2.2 (`ASM`, `LOAD`, `DDT`, `MAC`, `Z80.LIB`, and `ED80`, which
  is CP/M 2.2's `ED`). `ERA.CMD`, from Concurrent CP/M-86 2.0, because the 4.1 disks have none. `HELP` from
  Concurrent CP/M-86 2.0. The host bridge utilities `R`, `W` and `HDIR`. `VI` and `VI.DOC`, from
  [vi-cpm22](https://github.com/deltecent/vi-cpm22) V1.4 by Patrick Linstruth.
- **`B:`.** The demo programs `HELLO80`, `HELLO86` and `GFX`, with their source.
- **`D:`.** The XIOS 4.1D build kit, and the `XIOS.CON` of the disk's system. The kit has two changes for this machine. `GLOBAL.EQU` and
  `GLOBALMO.EQU` set the PC Video. `VSCPC.A86` has a fix for 640 x 200 graphics (its comments
  tell what changed).

Unused blocks and deleted directory entries are filled with `E5`, so no old data is on them.

**The disks are not under the MIT license.** Concurrent DOS, CP/M and their tools are Digital
Research's. The XIOS and the CompuPro files are CompuPro's (Viasyn). These files are under the MIT
license: `R.CMD`, `W.CMD` and `HDIR.CMD` (pascal100), `VI.COM` and `VI.DOC` ([vi-cpm22](https://github.com/deltecent/vi-cpm22),
Copyright (c) 2026 Patrick Linstruth), and the demo programs on `B:`.

**There is no undo.** Both disks are read/write. Copy them before you test writes.

## Known limits

- **Function 13 of `INT 10h` (read a dot) gives back nothing.** The other BIOS screen calls work.
- **`VI` does not run on console 0.** It needs a VT100. Use `ED` there.
- **Use `R`, `W` and `HDIR` on one console at a time.** The host bridge has one transfer stream.
  Two consoles at once were not tried.
- **Commands only from `A:` or the current drive.** `PATH.EXE` sets a `PATH`, but the shell does
  not use it to find commands.
