# Disks

A disk on an S-100 machine is three things:

- a **controller** board on the bus
- some **drives** on that controller
- a **disk image** in one of those drives

The controller is a board, and it goes in the machine file. The drives are units of the
controller. The disk image goes in a drive, from the machine file or with `MOUNT` at the
monitor. "Put a disk in the machine file", below, shows the two forms together.

This chapter is about the image, the drive and `MOUNT`. `MOUNT` works in the same way for every
controller. The examples use the `disk1`, the CompuPro 8″ floppy controller, because the CP/M
machines in the package and the Pascal-100 machine boot from it. The Boards chapter describes
every controller.

## The controller in this chapter

| Board | What it is | Drives | Ports |
|---|---|---|---|
| `disk1` | **CompuPro Disk 1**: an 8″ floppy controller with DMA and its own boot EPROM | 4 | C0 to C3 |

In the CompuPro machines, the controller has the id `fd0`. The drives are units of the board:
`drive0` to `drive3`.

## Put a disk in: `MOUNT`

```
MOUNT <id>[:<unit>] <file> [WP] [CREATE]
```

```
pascal100> MOUNT fd0:drive1 games.dsk
```

This puts the image `games.dsk` in drive 1 of the controller `fd0`. A relative path starts from
the machine's folder. The machines chapter gives the rule.

**The file must already exist.** A name that does not exist is a typing mistake, not a new disk.
The program tells you so, and does not make a disk that you did not want:

```
pascal100> MOUNT fd0:drive1 my-scratch.dsk
fd0: 'my-scratch.dsk': no such file
fd0: to make a blank one, add CREATE: MOUNT fd0:drive1 my-scratch.dsk CREATE
```

On the Disk 1, a blank disk also needs its size and sides, which only a machine file can give.
"Make a scratch disk", below, tells you how.

To take the disk out, type `UNMOUNT fd0:drive1`. The guest then sees an empty drive.

Board names are not case-sensitive. You must always give the drive, because a controller has
more than one.

## Put a disk in the machine file

`MOUNT` at the monitor is for the disk that you use *now*. A disk that belongs to a machine goes
in the machine file. The examples in the package do this. This is a complete machine file:

```toml
[machine]
name    = "mine"
startup = ["RUN 0"]

[[board]]
type     = "z80"
id       = "cpu0"
clock_hz = 0

[[board]]
type = "interfacer1"
id   = "if1"

  [board.unit.a]
  connect = "console"

[[board]]
type = "disk1"
id   = "fd0"
port = C0

  [[board.drive]]
  unit  = 0
  mount = "cpm22k-64k.dsk"     # relative to this file

  [[board.drive]]
  unit     = 1
  mount    = "games.dsk"
  readonly = true

[[board]]
type           = "memory"
id             = "mem0"
honors_phantom = "read"
ext            = true

  [[board.region]]
  type = "ram"
  at   = 0000
  size = "64K"
```

The drives are tables inside the `[[board]]` of the controller, `fd0`.

**Each `[[board.drive]]` is one `MOUNT` command.** The second one is
`MOUNT fd0:drive1 games.dsk WP`:

| At the monitor | In the machine file |
|---|---|
| `fd0` | `id = "fd0"` on the `[[board]]` |
| `drive1` | `unit = 1` in a `[[board.drive]]` |
| `games.dsk` | `mount = "games.dsk"` |
| `WP` | `readonly = true` |
| `CREATE` | `create = true` |

All the drives of one controller go below one `[[board]]`. Start a new `[[board.drive]]` for
each drive.

**Do not write a `type` on that `[[board]]`.** The `default` machine already has the controller
`fd0`. With a `type`, the file replaces the controller, and the settings that the base made on
it are lost. The configuring chapter gives the rule.

A drive can also have a `media` key, which `MOUNT` does not have. See "How the program finds the
format", below.

The file and the monitor do the same thing, at different times. The file puts the disk in the
drive when the machine starts. `MOUNT` changes it later.

The name has no folder, so the program looks for the file beside the machine file. For this
reason, you can copy the whole folder to another place, and it still boots.

`writeprotect = true` is the same as `readonly = true`. `CONFIG SAVE` saves `readonly`.

## Keep your disk safe

**Copy the folder before you experiment.** The program mounts a disk **read/write**, and every
write goes to the file on your computer at once. There is no undo. `SNAPSHOT` does not help,
because it saves the state of the machine, not the files on your computer. One wrong `ERA` can
destroy the example disk. The folder has everything that the machine needs, and the copy boots
from any place.

Read/write is the default because CP/M cannot save your work on a disk that it cannot write.

### Go back to the `A>` prompt before you stop

> **Do not `UNMOUNT`, copy or quit right after a file operation.**
> **Go back to the `A>` prompt first.**

Many CP/M BIOSes keep written data in a **buffer in memory**, and write it to the disk later,
for example when CP/M reads another track or waits for a key. At the `A>` prompt, CP/M waits for
a key, so a BIOS of this kind has then written its buffer. The simulator does not cause this.
The BIOS of a real machine did the same.

**Not every CP/M does this.** The author of each BIOS decided it. A *write-through* BIOS puts
every sector on the disk before it returns. You cannot tell from the `A>` prompt which kind you
have. On a write-through BIOS, the rule costs you nothing. On a buffered BIOS, it decides
whether your file is on the disk.

For example, `Ctrl-E` at the `A>` prompt is safe. `Ctrl-E` right after `ED` says that it wrote
your file is **not** safe.

### Write protection: `WP`

`WP` **write-protects the disk**, as the write-protect notch of a real diskette did. The guest
can read the disk. The controller refuses every write, and no write reaches the file on your
computer.

```
pascal100> MOUNT fd0:drive1 golden-master.dsk WP
fd0:drive1: mounted golden-master.dsk (write-protected)
```

In a machine file:

```toml
[[board]]
id = "fd0"

  [[board.drive]]
  unit     = 1
  mount    = "golden-master.dsk"
  readonly = true
```

`RO` is also accepted, and it means the same. `RO` is the word for a ROM socket. For a disk, the
correct word is `WP`. `SHOW MOUNTS` marks every write-protected disk:

```
pascal100> SHOW MOUNTS
  UNIT        KIND  HOLDS
  fd0:drive0  disk  cpm22k-64k.dsk
  fd0:drive1  disk  golden-master.dsk  (write-protected)
```

**Use `WP` only for a disk that the guest reads.** The controller tells the BIOS that the disk
is protected: the 765 refuses the write and sets its *not writable* status bit. But **CP/M has
no error for it.** A CP/M 2.2 BIOS write returns `0` for success or `1` for an error. For this
reason, a write to a `WP` disk shows as `Bdos Err On B: Bad Sector`.

The message `Bdos Err On A: R/O` comes from a different thing. It is CP/M's own read-only flag,
which `STAT A:=R/O` sets and a warm boot clears. The flag is in CP/M's memory, not on the disk.
A `WP` disk never causes it.

**Look for this line in `SHOW MOUNTS`.** If your computer does not let the program write the
file, because of its permissions, the disk still mounts, write-protected. The program tells you
when it mounts the disk, and `SHOW MOUNTS` says that you did not ask for it. CP/M will then fail
every write to that disk, and you think that it is writable.

## Make a scratch disk

A blank disk is an unformatted diskette. A format program in the guest makes it a disk. The
Disk 1 needs to know the size and the sides of a blank diskette, and only the machine file can
give them, with `media`.

1. Copy the CP/M example `cpm22-console.toml` to `scratch.toml` in the same folder. In the
   `[[board]]` of `fd0`, after the drive 0 table, add a blank disk in drive 1:

   ```toml
     [[board.drive]]
     unit   = 1
     mount  = "my-scratch.dsk"
     media  = "8ss"          # an 8-inch single-sided diskette
     create = true
   ```

   `create = true` makes the file when it is not there. `media = "8ds"` gives a two-sided
   diskette.

2. Start the machine. At the `A>` prompt, format drive B with CompuPro's **`FORMAT`**, which is
   on the CP/M disk in this package. Each answer is one key:

   ```
   A>FORMAT

   CompuPro Disk Sub-system FORMAT Version 2.2B

   Specify drive (A: - D:)  :B
      Disk is NOT formatted.
   Select Disk format mode (0,1,2,3):
   0 - 128          2 =  512
   1 = 256          3 = 1024        >3
   Confirm ready for format on disk drive B  (y).Y
   ...
   FORMAT complete.
   Do you want to FORMAT another disk ?
       R  -  request new parameters.
       cr -  duplicate last FORMAT.
       N  -  exit back to system.
       >N
   A>
   ```

   Mode 3 gives 1024-byte sectors, the format of the CP/M disk in the package. The file is now a
   625,920-byte disk.

3. Type `DIR B:`. CP/M answers `NO FILE`, and `STAT B:` shows 596K free. `B:` is ready for
   `PIP` and `SAVE`.

The program makes the file only when the file is not there. For this reason, the same machine
file loads the first time and every time after it, and the disk keeps its contents.

`MOUNT ... CREATE` at the monitor does not work on the Disk 1, because `MOUNT` cannot give
`media`. The program says so, and removes the empty file.

## How the program finds the format

You do not tell the program what kind of disk you mounted. It **looks at the number of bytes in
the file**. On an 8″ disk, track 0 of side 0 is always single density, 26 sectors of 128 bytes.
Every other track has the format of the disk:

| Format | Sectors on the other tracks | One side | Two sides |
|---|---|---|---|
| single density | 26 × 128 | **256,256** | **512,512** |
| double density | 26 × 256 | **509,184** | **1,021,696** |
| double density | 15 × 512 | **587,008** | **1,178,368** |
| double density | 8 × 1024 | **625,920** | **1,256,704** |

A file of 625,920 bytes is a single-sided disk with 1024-byte sectors, and it cannot be anything
else. The CP/M disk in the package and a disk from CompuPro's `FORMAT` in mode 3 have this size.
The UCSD Pascal disks of the Pascal-100 have 15 sectors of 512 bytes (587,008 bytes).

**The Disk 1 has 8″ drives only.** It refuses an image the size of a 5¼″ disk.

**A file smaller than every format is a blank disk.** It needs `media` in the machine file, as
in "Make a scratch disk". A file larger than every format is refused.

**`media` sets the diskette yourself.** `8ss` and `8ds` give the size and the sides. A full name
such as `8ss-dd1024` also gives the format of the tracks. Use it on the drive in a machine file.
`media` is a key in the machine file only. You cannot set it with `MOUNT`. **For a disk that
has data on it, leave `media` out.** A wrong `media` gives disk errors in the guest, and the
troubleshooting chapter tells you why.

## ImageDisk files: `.imd`

Much archived 8″ and 5¼″ software is in the **ImageDisk** format. A controller reads raw
sectors, so the program does not mount an `.imd` directly. When you `MOUNT` a file whose name
ends in `.imd`, the program converts it to a raw image beside it, with the name `.dsk`, and
mounts the `.dsk`:

```
pascal100> MOUNT fd0:drive0 cpm.imd
```

The program prints the comment line of the `.imd` and the size of the new `.dsk`. Read the
comment line, and check that it is the disk that you expected.

**The program never writes over a `.dsk` that is already there.** If `cpm.dsk` exists, the
program tells you to remove it or to mount it directly. For this reason, a second mount cannot
lose the changes that you made. After the conversion, the `.dsk` is the disk.

The conversion happens only in the `MOUNT` command. A `mount = "cpm.imd"` line in a machine file
does not convert. `MOUNT` the `.imd` one time, and then put the `.dsk` in the machine file.

## A disk on a TNFS server

A disk image can be on a **TNFS server**, the network file system of the FujiNet project:

```
pascal100> MOUNT fd0:drive0 tnfs://fileserver/cpm/games.dsk
pascal100> MOUNT fd0:drive1 tnfs://fileserver:16384/scratch.dsk WP
```

The form is `tnfs://<host>[:<port>]/<path>`. The default port is **16384**.

In a machine file:

```toml
[[board]]
id = "fd0"

  [[board.drive]]
  unit  = 0
  mount = "tnfs://fileserver/cpm/games.dsk"

  [[board.drive]]
  unit     = 1
  mount    = "tnfs://fileserver:16384/scratch.dsk"
  readonly = true
```

A `tnfs://` name does not start from the machine's folder, because it names its server.

The program gets the whole image **one time, when you mount it**. After that, the disk works
like a local disk. The program writes your changes back to the server when the guest flushes,
and when you `UNMOUNT` the disk. `WP` works as on a local disk. If the server does not let the
program write the file, the disk mounts write-protected, as a local file does.

**If the server stops while you use the disk, the program tells you.** It prints a line that
says that it keeps the changes in memory only. The guest continues to run, and the program
continues to try. When the server comes back, the program says so, and saves the changes. If you
quit before that, the changes are lost.

TNFS carries only **single-file images**, such as the floppy images of this chapter.

## Simulate a bad sector: `FAULT`

`FAULT` makes a sector of a mounted disk bad. Use it to test the retry code and the error
messages of a guest. The disk image does not change. The faults are in the drive, beside the
image.

These controllers take faults: `disk1`, `disk1a`, `tarbell`, `tarbelldd` and `jadedd`. Another
controller refuses the command:

```
pascal100> FAULT hd0:drive0 ADD 2/0/1 data
hd0:drive0: the disk3 board does not take faults on its disk
```

### Give a fault

A fault is a sector address, a kind and an optional count:

```
<track>/<head>/<sector> <kind> [count=N]
```

- The track, the head and the sector are decimal numbers. Use `*` for all tracks, all heads or
  all sectors.
- The sector is the number that the guest asks for. On most soft-sector controllers, the first
  sector of a track is 1.
- With no count, the fault is **hard**. The sector fails each time.
- With `count=N`, the sector fails N times. After that, the sector is good. A test with a count
  shows that a retry gets past the error.

| Kind | What the controller sees |
|---|---|
| `data` | The data field has a CRC error. The guest still gets the data. |
| `id` | The ID field has a CRC error. The controller does not find the sector. |
| `missing` | The sector has no ID field. The controller does not find the sector. |
| `write` | The drive does not write the sector. The controller reports a write fault. |

The controller reports each fault with the status bits of its chip. The `disk1` and `disk1a` have
a µPD765. The other controllers have a WD17xx.

| Kind | WD17xx status | µPD765 result |
|---|---|---|
| `data` | CRC Error (bit 3) | ST0 AT, ST1 DE, ST2 DD |
| `id` | Record Not Found and CRC Error (bits 4 and 3) | ST0 AT, ST1 DE |
| `missing` | Record Not Found (bit 4) | ST0 AT, ST1 ND |
| `write` | Write Fault (bit 5) | ST0 AT and EC |

1. Mount the disk.
2. Add the fault. This one gives two CRC errors on track 2, head 0, sector 1:

   ```
   pascal100> FAULT fd0:drive0 ADD 2/0/1 data count=2
   fd0:drive0: fault 2/0/1 data count=2
   ```

3. Run the guest.
4. Type `FAULT` to see the faults of all the drives:

   ```
   pascal100> FAULT
     UNIT        FAULT                HITS   LEFT
     fd0:drive0  2/0/1 data count=2      2      0
   ```

`HITS` is the number of times that the fault occurred. `LEFT` is the number of failures before
the sector is good. `LEFT` is `-` for a hard fault. On this disk, the sector holds the start of the
CP/M directory. The BIOS got two CRC errors, read the sector a third time, and continued. With a
hard fault, the CompuPro BIOS tries ten times, and CP/M prints `Bdos Err On A: Bad Sector`.

A second `ADD` with the same address and kind replaces the fault, and sets its counts again.
`SHOW MOUNTS` shows the number of faults beside each disk:

```
pascal100> SHOW MOUNTS
  UNIT        KIND  HOLDS
  fd0:drive0  disk  CPM22N.DSK  (2 faults)
```

### Remove a fault

| Command | What it removes |
|---|---|
| `FAULT fd0:drive0 CLEAR 2/0/1` | each fault at that address |
| `FAULT fd0:drive0 CLEAR 2/0/1 data` | the `data` fault at that address |
| `FAULT fd0:drive0 CLEAR ALL` | each fault of the drive |

`CLEAR` matches the address as you wrote it. `CLEAR 10/*/*` removes the fault `10/*/*`. It does
not remove a fault on one sector of track 10.

`MOUNT` and `UNMOUNT` remove the faults of the drive, because the faults are for the disk in it.

### See each fault as it occurs

Turn on the `fault` debug flag of the controller:

```
pascal100> SET fd0 DEBUG=fault
```

The controller then writes a line each time that a fault occurs. The line starts with the
address of the instruction that the processor ran:

```
ED04  fd0: fault drive0 2/0/1 data hit 1, 1 left
ED04  fd0: fault drive0 2/0/1 data hit 2, 0 left
```

The flag writes nothing until you turn it on. `SET fd0 NODEBUG=fault` turns it off. The flag
changes only the lines. The faults occur and count in the same way with the flag off.

### Faults in the machine file

The `faults` key of a drive gives its faults. Put a `;` between two faults:

```toml
[[board]]
type = "disk1"
id   = "fd0"

  [[board.drive]]
  unit   = 0
  mount  = "CPM22N.DSK"
  faults = "2/0/1 data count=2; 10/*/* write"
```

The machine starts with the faults on the disk. `CONFIG SAVE` writes the `faults` key of each
drive that has faults. A snapshot keeps the faults and their counts, and `RESTORE` gives them
back.

## Look at what is in the machine

`SHOW` on the controller lists its drives and the disk in each one. `BOARDS` lists every board
and the addresses that it decodes:

```
pascal100> SHOW fd0
pascal100> BOARDS
```

These two commands answer most questions about a machine that does not boot. The most common
answer is that the disk is in the wrong drive.
