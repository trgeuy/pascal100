# CompuPro CP/M 8-16 1.1PD from a Disk 3 hard disk

**To start it**, name the machine file from inside this folder:

```
$ pascal100 cpm816.toml
```

On **Windows** the program is `pascal100.exe`. The machine boots by itself. A terminal window opens
and shows the CP/M prompt:

```
CP/M 8-16  1.1h loader --
...
CompuPro CP/M 8-16 1.1PD
NO M-DRIVE/H Active
Total Memory: 1 MEGABYTE
A>
```

`cpm816-console.toml` is the same machine with the console on the terminal where you
started the program. Use it on a build with no window.

`^E` (STOP) gives the keyboard back to the monitor at any time. `RUN` continues the guest.

**Go back to the `A>` prompt before you stop or quit.** A BIOS can keep written data in memory
for a short time ([Keep your disk safe](../../docs/manual/disks.md#go-back-to-the-a-prompt-before-you-stop)).

## Which CompuPro system is this?

CompuPro sold three operating systems for the CPU 8085/88 card. This example is the second:

| System | Example | Users | Runs |
|---|---|---|---|
| CP/M-86 | [`cpm86`](../cpm86/) (on the CPU 86/87) | one | 8086 programs (`.CMD`) only |
| **CP/M 8-16** | `cpm8-16` | one | 8086 programs (`.CMD`) and 8080 programs (`.COM`) |
| MP/M 8-16 | [`mpm8-16`](../mpm8-16/) | several | 8086 and 8080 programs, each user at a terminal |

CP/M 8-16 is CP/M-86 with CompuPro's `SW!` (SWITCH). A `.CMD` program runs on the 8088. A `.COM`
program runs on the 8085, through `SW!`. A `.COM` program must use 8080 or 8085 instructions: the
8085 does not run Z80 instructions.

To try it, put the CP/M 2.2 disk of [`cpm22`](../cpm22/) in the second floppy drive. CP/M 8-16
calls it `G:`. At the `A>` prompt, type `^E` for the monitor, then:

```
pascal100> MOUNT fd0:drive1 "../cpm22/cpm22k-64k.dsk"
pascal100> RUN
```

Then type `G:STAT G:*.COM` at the `A>` prompt. CP/M 2.2's own `STAT.COM` runs unchanged and lists
the `.COM` files on `G:`. The machine writes to a disk that you mount, so mount a copy if you want
to keep the original unchanged.

## The drives

The machine boots from the floppy, but `A:` is the hard disk. The CompuPro BIOS divides the
40 MB Quantum Q540 into five drives:

| Drive | Disk | Free space (`STAT`) |
|---|---|---|
| `A:` | hard disk, cylinders 3-74 | 4,816K |
| `B:`, `C:`, `D:` | hard disk, 140 cylinders each | 10,016K each |
| `E:` | hard disk, cylinders 495-511 | 1,216K |
| `F:` | the boot floppy (`cpm816pd-boot.dsk`), floppy drive 0 | 922K |
| `G:` | floppy drive 1 (`fd0:drive1`), empty at the start | |

`A:` and `F:` hold the same files. `B:` to `E:` are empty.

## The disks

| File | What it is |
|---|---|
| `cpm816pd-boot.dsk` | The boot floppy. `CPM.SYS` is CompuPro's `CPM340.SYS`: CP/M 8-16 V1.1PD for one Q540 on a Disk 3. |
| `q540.dsk` | The Q540 hard disk, 37,748,736 bytes: 512 cylinders, 8 heads, 9 sectors of 1024 bytes. |

Both disks hold the V1.1PD file set: the DRI programs, CompuPro's `SW!`, `DISK3`, `CLOCK`,
`MFORM` and the other CompuPro tools, and CompuPro's `READ.ME`. `R.CMD`, `W.CMD` and `HDIR.CMD`
are the 8086 host bridge tools ([Moving files in and out](../../docs/manual/file-transfer.md)).
The BIOS sources (`.A86`) and the other `CPMxxx.SYS` systems are not on the disks.

**The disks are not under the MIT license.** CP/M-86 is Digital Research's, and the CompuPro files
are CompuPro's. Only `R.CMD`, `W.CMD` and `HDIR.CMD` are pascal100's, under the MIT license.

We made the disks in pascal100 with CompuPro's own programs:

1. `DISK3 FORMAT Q540` formatted a new, empty `q540.dsk`. It wrote the CompuPro label, the
   drive's SPECIFY block and the bad-track map, with 21 reserved tracks.
2. `PIP` copied `CPM340.SYS` to `CPM.SYS` on the floppy.
3. `PIP` copied the file set to both disks.

We have no V1.1PD disk image. The floppy's boot tracks are from the CompuPro CP/M 8-16 1.1R System
Master (Dave Dunfield's archive, `cpro8588/DISK1.IMD`), so the first line says `1.1h loader`. The loader starts `CPM.SYS` and
does not depend on its version.

`MFORM` formats a CompuPro M-Drive/H memory disk. The BIOS runs it at each start. This machine has
no M-Drive/H, so it does nothing.

## The machine

A CompuPro CPU 8085/88, an Interfacer 1 for the console, a CompuPro **Disk 3** hard disk controller
at `90`, a Disk 1 floppy controller at `C0`, a System Support 1 at `50`, the host bridge at `B0` and
1 MB of RAM. The Disk 1 boot ROM (routine 4) runs on the 8085 and reads the boot sector. That code
does `IN FD`: the board gives the bus to the 8088, which loads `CPM.SYS` and starts CP/M 8-16. The board document is
[`docs/boards/compupro-disk3.md`](../../docs/boards/compupro-disk3.md).

The Disk 3 does all its work by DMA: the BIOS writes a command block in memory and sends one
byte to port `90`. The board reads the block, moves the sector and writes the status back.

**The host bridge.** `R`, `W` and `HDIR` copy files between a CP/M disk and the folder you started
pascal100 in. They are the 8086 versions, `R.CMD`, `W.CMD` and `HDIR.CMD`
([Moving files in and out](../../docs/manual/file-transfer.md)).
