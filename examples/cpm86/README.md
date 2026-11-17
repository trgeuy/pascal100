# CompuPro CP/M-86 1.1 on the CPU 8085/88

**To start it**, name the machine file from inside this folder:

```
$ pascal100 cpm86.toml
```

On **Windows** the program is `pascal100.exe`. The machine boots by itself. A terminal window opens
and shows the CP/M prompt:

```
CompuPro CP/M 86 1.1PD

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

CompuPro sold three operating systems for the CPU 8085/88 card. This example is the first:

| System | Example | Users | Runs |
|---|---|---|---|
| **CP/M-86** | `cpm86` | one | 8086 programs (`.CMD`) only |
| CP/M 8-16 | [`cpm8-16`](../cpm8-16/) | one | 8086 programs (`.CMD`) and 8080 programs (`.COM`) |
| MP/M 8-16 | [`mpm8-16`](../mpm8-16/) | several | 8086 and 8080 programs, each user at a terminal |

CP/M-86 is Digital Research's CP/M for the 8086 family. On the CPU 8085/88 the 8085 only starts the
boot, then hands the bus to the 8088 for good. An 8080 `.COM` program does not run here. CP/M 8-16
adds CompuPro's `SW!`, which runs it on the 8085.

## The machine

A CompuPro CPU 8085/88, an Interfacer 1 for the console, a Disk 1 floppy controller at `C0`, a
System Support 1 at `50` for its clock (`CLOCK` shows the host's time; the serial port is not
connected), the host bridge at `B0` and 1 MB of RAM. The Disk 1 boot ROM (routine 4) runs on the 8085 and reads the boot sector. That code
does `IN FD`: the card gives the bus to the 8088, which starts at `FFFF0`, loads `CPM.SYS` and
starts CP/M-86.

**The host bridge.** `R`, `W` and `HDIR` copy files between the CP/M disk and the folder you
started pascal100 in. They are the 8086 versions, `R.CMD`, `W.CMD` and `HDIR.CMD`
([Moving files in and out](../../docs/manual/file-transfer.md)).

## The files

| File | What it is |
|---|---|
| `cpm86.toml` | The whole machine. The console is a built-in terminal window. Needs a build with SDL3. |
| `cpm86-console.toml` | The same machine and disk, with the console on stdio. |
| `cpm86-11.dsk` | The system disk: CompuPro CP/M-86 1.1PD, System Master disk 1 of 2 (serial C86-272-1854), from Dave Dunfield's archive (`CCPM86A.IMD`). |

The disk holds these programs:

- **Digital Research CP/M-86:** `DDT86`, `GENCMD`, `LOADER`, `PIP`, `STAT`.
- **CompuPro:** `FORMAT`, `COPY`, `MFORM` (the M-Drive/H), `CLOCK`, `SYSGEN88`, and system files
  (`CPM*.SYS`) for other disk controllers, with the BIOS source `GBCBIOS1.A86` and `GBCBIOS2.A86`.
- `README`: CompuPro's notes on changing the floppy step rate.
- **pascal100:** `R.CMD`, `W.CMD` and `HDIR.CMD`, the host bridge utilities.

The disk is a copy of the master, with the three host bridge utilities added. Then the free space was changed: unused blocks and deleted
directory entries are filled with `E5`, so no old data is on the disk.

**The disk is not under the MIT license.** CP/M-86 is Digital Research's, and the CompuPro files
are CompuPro's. Only `R.CMD`, `W.CMD` and `HDIR.CMD` are pascal100's, under the MIT license.

**There is no undo.** Drive 0 is read/write. Copy the image before you test writes.
