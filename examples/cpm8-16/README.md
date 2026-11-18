# CompuPro CP/M 8-16 1.1R on the CPU 8085/88

**To start it**, name the machine file from inside this folder:

```
$ pascal100 cpm816.toml
```

On **Windows** the program is `pascal100.exe`. The machine boots by itself. A terminal window opens
and shows the CP/M prompt:

```
CompuPro Systems CP/M 8-16  vers 1.1R

   System Memory:  2047K
   TPA Base = 961:0000
...
A>
```

`cpm816-console.toml` is the same machine with the console on the terminal where you started the
program. Use it on a build with no window.

`^E` (STOP) gives the keyboard back to the monitor at any time. `RUN` continues the guest.

**Go back to the `A>` prompt before you stop or quit.** A BIOS can keep written data in memory
for a short time ([Keep your disk safe](../../docs/manual/disks.md#go-back-to-the-a-prompt-before-you-stop)).

## Which CompuPro system is this?

CompuPro sold three operating systems for the CPU 8085/88 card. This example is the second:

| System | Example | Users | Runs |
|---|---|---|---|
| CP/M-86 | [`cpm86`](../cpm86/) | one | 8086 programs (`.CMD`) only |
| **CP/M 8-16** | `cpm8-16` | one | 8086 programs (`.CMD`) and 8080 programs (`.COM`) |
| MP/M 8-16 | [`mpm8-16`](../mpm8-16/) | several | 8086 and 8080 programs, each user at a terminal |

CP/M 8-16 is CP/M-86 with CompuPro's `SW!` (SWITCH). A `.CMD` program runs on the 8088. A `.COM`
program runs on the 8085, through `SW!`: CP/M 2.2's own `STAT.COM` runs unchanged from a CP/M 2.2
disk in drive B. The host bridge utilities have 8086 versions on this disk, so `SW!` is not needed
for them. A `.COM` program must use 8080 or 8085
instructions: the 8085 does not run Z80 instructions.

To try it, put the CP/M 2.2 disk of [`cpm22`](../cpm22/) in drive 1. At the `A>` prompt, type `^E`
for the monitor, then:

```
pascal100> MOUNT fd0:drive1 "../cpm22/cpm22k-64k.dsk"
pascal100> RUN
```

Then type `B:STAT B:*.COM` at the `A>` prompt.

## The machine

A CompuPro CPU 8085/88, an Interfacer 1 for the console, a Disk 1 floppy controller at `C0`, a
System Support 1 at `50` for its clock (`CLOCK` shows the host's time; the serial port is not
connected), the host bridge at `B0` and 1 MB of RAM. The Disk 1 boot ROM (routine 4) runs on the 8085 and reads the boot sector. That code
does `IN FD`: the card gives the bus to the 8088, which loads `CPM.SYS` and starts CP/M 8-16.

**The host bridge.** `R`, `W` and `HDIR` copy files between the CP/M disk and the folder you
started pascal100 in. They are the 8086 versions, `R.CMD`, `W.CMD` and `HDIR.CMD`
([Moving files in and out](../../docs/manual/file-transfer.md)).

The loader waits until the console's DSR or CTS line reads OFF. The console channel has
`rs232_inputs = "open"`: a 3-wire cable, with nothing on those inputs. The banner says 2047K on a
1 MB machine. That is a bug in the CompuPro BIOS, not in the sim: the sign-on code in
`TMXBIOS.A86` converts paragraphs to K with `ROR AX,1`. Its comment says the shift recovers the carry,
which is what `RCR` does. With all 1 MB present the paragraph total is FFFFh, and ROR moves its
low bit into bit 15 (FFFFh, then 7FFh = 2047). Less memory gives an even total and the right
number: with a gap at 512K the banner shows 512K.

`TOD` with no date prints a blank line (NUL characters) after a boot, and a time that you set with
`TOD MM/DD/YY HH:MM:SS` does not advance. That is also how CP/M 8-16 1.1R works, not a fault in the
sim. `TOD` keeps its date as text in system memory, and only `TOD` writes it. The BIOS does not read
the System Support 1 clock and has no clock tick: `ACTIVE.EQU` sets `INTRACT EQU FALSE`, and the
timer vectors in `TMXINTIO.INI` all go to `ITRAP`. `TOD` also refuses a year such as `26`; `86`
works. For the real time, use `CLOCK`.

## The files

| File | What it is |
|---|---|
| `cpm816.toml` | The whole machine. The console is a built-in terminal window. Needs a build with SDL3. |
| `cpm816-console.toml` | The same machine and disk, with the console on stdio. |
| `cpm816-11r.dsk` | The system disk: CompuPro CP/M 8-16 1.1R System Master, from Dave Dunfield's archive (`cpro8588/DISK1.IMD`), double-sided. |

The disk holds these programs:

- **Digital Research CP/M-86:** `ASM86`, `DDT86`, `ED`, `GENCMD`, `GENDEF`, `HELP` (with
  `HELP.HLP`), `LMCMD`, `PIP`, `STAT`, `SUBMIT`.
- **CompuPro:** `SW!`, `FORMAT`, `COPY`, `COPYDISK`, `MFORM` (the M-Drive/H), `CLOCK`, `TOD`,
  `SYSGEN` with the loader `LDR88`, and the system build files `CPM816.H86`, `LDBDOS.H86` and
  `LDCPM.H86`.
- **pascal100:** `R.CMD`, `W.CMD` and `HDIR.CMD`, the host bridge utilities. They were added
  after the files below were removed.

**What was removed from the master.** The original disk also held programs that are not Digital
Research's or CompuPro's: the Manx Aztec C compiler (`C86`, `AS86`, `LN`, its libraries and
headers), the MINCE editor, `CRC.COM`, and some test files (`EXMPL.*`, `VT100.*`, `$TMP.$$$`).
They were erased with CP/M's `ERA`. Then the free space was filled with `E5`, so no old data is on
the disk.

**The disk is not under the MIT license.** CP/M-86 is Digital Research's, and the CompuPro files
are CompuPro's. Only `R.CMD`, `W.CMD` and `HDIR.CMD` are pascal100's, under the MIT license.

**There is no undo.** Drive 0 is read/write. Copy the image before you test writes.
