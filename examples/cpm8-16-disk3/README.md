# CompuPro CP/M 8-16 1.1PD from a Disk 3 hard disk

**To start it**, name the machine file from inside this folder:

```
$ pascal100 cpm816-disk3.toml
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

`cpm816-disk3-console.toml` is the same machine with the console on the terminal where you
started the program. Use it on a build with no window.

`^E` (STOP) gives the keyboard back to the monitor at any time. `RUN` continues the guest.

**Go back to the `A>` prompt before you stop or quit.** A BIOS can keep written data in memory
for a short time ([Keep your disk safe](../../docs/manual/disks.md#go-back-to-the-a-prompt-before-you-stop)).

## The drives

The machine boots from the floppy, but `A:` is the hard disk. The CompuPro BIOS divides the
40 MB Quantum Q540 into five drives:

| Drive | Disk | Free space (`STAT`) |
|---|---|---|
| `A:` | hard disk, cylinders 3-74 | 4,816K |
| `B:`, `C:`, `D:` | hard disk, 140 cylinders each | 10,016K each |
| `E:` | hard disk, cylinders 495-511 | 1,216K |
| `F:` | the boot floppy (`cpm816pd-boot.dsk`) | 922K |

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

We made the disks in pascal100 with CompuPro's own programs:

1. `DISK3 FORMAT Q540` formatted a new, empty `q540.dsk`. It wrote the CompuPro label, the
   drive's SPECIFY block and the bad-track map, with 21 reserved tracks.
2. `PIP` copied `CPM340.SYS` to `CPM.SYS` on the floppy.
3. `PIP` copied the file set to both disks.

We have no V1.1PD disk image. The floppy's boot tracks are from the CP/M 8-16 1.1R master in
[`cpm8-16`](../cpm8-16/), so the first line says `1.1h loader`. The loader starts `CPM.SYS` and
does not depend on its version.

`MFORM` formats a CompuPro M-Drive/H memory disk. The BIOS runs it at each start. This machine has
no M-Drive/H, so it does nothing.

## The machine

The machine is the [`cpm8-16`](../cpm8-16/) machine with a CompuPro **Disk 3** at `90`: a CPU
8085/88, an Interfacer 1 for the console, a Disk 1 floppy controller at `C0`, a System Support 1
at `50`, the host bridge at `B0` and 1 MB of RAM. The board document is
[`docs/boards/compupro-disk3.md`](../../docs/boards/compupro-disk3.md).

The Disk 3 does all its work by DMA: the BIOS writes a command block in memory and sends one
byte to port `90`. The board reads the block, moves the sector and writes the status back.
