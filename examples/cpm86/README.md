# CompuPro CP/M-86 1.1 on the CPU 86/87

**To start it**, name the machine file from inside this folder:

```
$ pascal100 cpm86.toml
```

On **Windows** the program is `pascal100.exe`. The machine boots by itself. A terminal window opens
and shows the CP/M prompt:

```
CP/M 86 1.1PAA
...
Loading CPM.SYS
...
NO M-DRIVE/H Active
Total Memory: 1 MEGABYTE

A>
```

`cpm86-console.toml` is the same machine with the console on the terminal where you started the
program. Use it on a build with no window.

`^E` (STOP) gives the keyboard back to the monitor at any time. `RUN` continues the guest.

**Go back to the `A>` prompt before you stop or quit.** A BIOS can keep written data in memory
for a short time ([Keep your disk safe](../../docs/manual/disks.md#go-back-to-the-a-prompt-before-you-stop)).

## Which CompuPro system is this?

CompuPro sold three operating systems for its 8086 family cards. This example is the first:

| System | Example | Users | Runs |
|---|---|---|---|
| **CP/M-86** | `cpm86` | one | 8086 programs (`.CMD`) only |
| CP/M 8-16 | [`cpm8-16`](../cpm8-16/) | one | 8086 programs (`.CMD`) and 8080 programs (`.COM`) |
| MP/M 8-16 | [`mpm8-16`](../mpm8-16/) | several | 8086 and 8080 programs, each user at a terminal |

CP/M-86 is Digital Research's CP/M for the 8086 family. It runs 8086 programs only, so this
example uses the **CPU 86/87**, CompuPro's 8086 card. An 8080 `.COM` program does not run here.
CP/M 8-16 and MP/M 8-16 run 8080 programs too: their examples use the CPU 8085/88, which has an
8085 for that (`SW!`).

## The machine

A CompuPro CPU 86/87 (an 8086, no 8087), an Interfacer 1 for the console, a Disk 1 floppy
controller at `C0`, a System Support 1 at `50` for its clock (`CLOCK` shows the host's time; the
serial port is not connected), the host bridge at `B0` and 1 MB of RAM.

**How it boots.** The 8086 starts at `FFFF0`. While the Disk 1 boots, its boot ROM answers every
memory read, so the 8086 runs the ROM's routine 0 (console on the Interfacer 1). The routine reads
the boot tracks. The loader on them loads `CPM.SYS` and starts CP/M-86. The startup line is
`RUN FFFF0`, the 8086's reset address, not the `RUN 0` of the 8080 examples. Over MCP, boot with
`run {from: 1048560, until: "A>"}` (`from` takes a decimal number).

**The host bridge.** `R`, `W` and `HDIR` copy files between the CP/M disk and the folder you
started pascal100 in. They are the 8086 versions, `R.CMD`, `W.CMD` and `HDIR.CMD`
([Moving files in and out](../../docs/manual/file-transfer.md)).

## The files

| File | What it is |
|---|---|
| `cpm86.toml` | The whole machine. The console is a built-in terminal window. Needs a build with SDL3. |
| `cpm86-console.toml` | The same machine and disk, with the console on stdio. |
| `cpm86-11.dsk` | The system disk: CompuPro CP/M-86 1.1PAA, System Master disk 2 of 2, from vintagecomputer.net (`CPM11PA2.IMD`). |

Disk 1 of this master does not boot on the CPU 86/87: its boot tracks hold the loader for the CPU
8085/88. Disk 2 has the 8086 loader. The CPU 86/87 manual says to boot Disk 2.

The disk holds these programs:

- **Digital Research CP/M-86:** `ASM86`, `DDT86`, `ED`, `GENCMD`, `GENDEF`, `HELP`, `LDCOPY`,
  `LMCMD`, `LOADER`, `PIP`, `STAT`, `SUBMIT`.
- **CompuPro:** `FORMAT`, `COPY`, `COPYDISK`, `MFORM` (the M-Drive/H), `DISK2` (the Disk 2 hard
  disk, with `DISK2.DOC`), `CLOCK`, `TOD`, `SYSGEN86`, and the system files `CPM.SYS` and
  `CPM.H86`.
- **The BIOS source:** `GBCBIOS*.A86`, `GBCROM.A86` and `GBCBOOT.ASM`, with the files to build
  the system and the loader (`LDBDOS.H86`, `LDBIOS.H86`, `LDCPM.H86`, `LOADER.H86`, `MAKSYS.SUB`,
  `MAKLDR.SUB`, `COPYSYS.COM`).
- **pascal100:** `R.CMD`, `W.CMD` and `HDIR.CMD`, the host bridge utilities.

The disk is a copy of the master, with the three host bridge utilities added. Then the free space
was changed: unused blocks and deleted directory entries are filled with `E5`, so no old data is
on the disk.

**The disk is not under the MIT license.** CP/M-86 is Digital Research's, and the CompuPro files
are CompuPro's. Only `R.CMD`, `W.CMD` and `HDIR.CMD` are pascal100's, under the MIT license.

**There is no undo.** Drive 0 is read/write. Copy the image before you test writes.
