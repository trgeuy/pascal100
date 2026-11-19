# Quick start: CP/M in one command

This chapter boots CompuPro CP/M 2.2K on the default machine: a Z80, an Interfacer 1 console, a
Disk 1 floppy disk controller and 64K of memory. It also shows you the three commands that you
use every time: `Ctrl-E`, `RUN` and `QUIT`.

Type the commands in the folder where you unzipped the package.

## Boot CP/M

1. Start the machine:

   ```
   $ ./pascal100 examples/cpm22/cpm22-console.toml
   ```

   On **Windows**, the program is `pascal100.exe`, and the path uses backslashes:

   ```
   > pascal100.exe examples\cpm\cpm22-console.toml
   ```

   The machine boots CP/M:

   ```
   pascal100 X.Y.Z -- z80, full speed.
   machine: cpm22-console.  HELP for commands.
   startup> RUN 0
   [console -- ^E returns to the monitor]
   CompuPro Systems
   64K CP/M 2.2K

   A>
   ```

2. Type `DIR` to see what is on the disk:

   ```
   A>DIR
   A: MAC      COM : Z80      LIB : LS       COM : R        COM
   A: W        COM : HDIR     COM : FUJIDIR  COM : FUJIGET  COM
   A: FUJIPUT  COM : WGET     COM : PIP      COM : ED       COM
   A: SUBMIT   COM : XSUB     COM : ASM      COM : DDT      COM
   A: LOAD     COM : STAT     COM : DUMP     COM : FORMAT   COM
   A: COPY     COM : SYSGEN   COM
   A>
   ```

   The disk has the Digital Research tools (`ASM`, `MAC`, `DDT`, `ED`, `PIP`), the CompuPro
   `FORMAT` and `COPY`, and `R`, `W` and `HDIR`, which copy files between CP/M and your
   computer.

3. Type `STAT` to see the free space on the disk:

   ```
   A>STAT
   A: R/W, Space: 488k
   ```

## Stop, continue and leave

1. Press **`Ctrl-E`**. The machine stops, and you get the monitor:

   ```
   A>
   STOP -- the machine is still at F4BD. RUN resumes.
   C0Z1S0P1H1N0 A=00 BC=007F DE=DD06 HL=E80E SP=E837 IFF1=0 PC=F4BD  IN A,(01)
   C0Z0S0P0H0N0 A'00 BC'0000 DE'0000 HL'0000 IX=0000 IY=0000 I=00 IM=00 IFF2=0
   pascal100>
   ```

   The BIOS was waiting for a key, so the machine stopped in the loop that reads the console.

2. Type `RUN`. The machine continues from the instruction where it stopped. Your `A>` prompt
   is where you left it:

   ```
   pascal100> RUN
   ```

3. Press `Ctrl-E` again, and type `QUIT` to leave the program:

   ```
   pascal100> QUIT
   ```

These are the three commands to remember:

| | |
|---|---|
| **`Ctrl-E`** | Stop the machine, and go to the monitor. The machine loses nothing. |
| **`RUN`** | Start the machine again, from where it stopped. |
| **`QUIT`** | Leave the program. `Q` is enough. There is no `EXIT` command. |

## What `Ctrl-E` does

**The machine stops at the instruction where it was, and it loses nothing.** A stop is not a
reset. The registers, the memory, the disk in the drive and the program counter do not change.
While the machine is stopped, you can read memory, step one instruction at a time and set
breakpoints.

`Ctrl-C` is not the stop key, because **`Ctrl-C` belongs to the guest**. CP/M does a warm boot
when you press `Ctrl-C`. The program reads `Ctrl-E` before the guest can get it, and every
other key goes to the guest. If a guest needs `Ctrl-E`, type `SET CONSOLE stop=1D` at the
monitor. The stop key is then `Ctrl-]`.

## Before you change the disk

**Copy the folder before you write to the disk.** The machine mounts the disk image
read/write. Everything that you do in CP/M changes the file on your computer, and the program
keeps no copy. The folder has all that the machine needs, and the copy boots from any place:

```
$ cp -R examples/cpm22 my-cpm
$ ./pascal100 my-cpm/cpm22-console.toml
```

**Go back to the `A>` prompt before you quit or copy the image.** Then the programs that wrote
to the disk have finished, and CP/M has written their files.

To look at a disk that you do not want to change, write-protect it at the monitor:

```
pascal100> MOUNT fd0:drive0 cpm22k-64k.dsk WP
fd0:drive0: mounted cpm22k-64k.dsk (write-protected)
```

The controller then refuses every write. CP/M does not know that the disk is write-protected,
and a program that tries to write can fail. Do not use `WP` for a disk that you save work on.
The disks chapter tells you more.

## What happened when it booted

The machine file names the built-in `default` machine, puts the disk image in drive 0 of the
Disk 1 (`fd0`), and types `RUN 0` for you. The `startup>` line shows this command.

**There is no `BOOT` command.** After a reset, the boot EPROM of the Disk 1 answers at address
0. `RUN 0` starts it, as the reset switch did on the real machine. A machine file can do only
what you can type at the monitor.

The rest of the boot is real software. The EPROM reads track 0 of drive 0 into memory. That code
loads CP/M, and the BIOS of CP/M prints its banner.

The machine file names its disk `cpm22k-64k.dsk`, with no folder. The program looks for the
disk in the folder of the machine file. For this reason, the copy in `my-cpm` finds its own
disk.

## Where to go next

- **Other machines.** Type `./pascal100 --list` to see the built-in machines. The `pascal100`
  machine is the Digicomp Pascal-100, which boots UCSD Pascal III.0 on the MicroEngine from a
  disk that you supply. The machines chapter describes them.
- **Moving files.** The chapter on moving files in and out describes `R`, `W` and `HDIR`.
- **Complete sessions.** The worked examples chapter has more, from start to end.
