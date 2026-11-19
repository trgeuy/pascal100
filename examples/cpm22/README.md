# CompuPro CP/M 2.2K on the default machine

**To start it**, name the machine file from inside this folder:

```
$ pascal100 cpm22.toml
```

On **Windows** the program is `pascal100.exe`. The machine boots by itself. A VT100 terminal
window opens and stops at the CP/M prompt. You do not type a boot command. Type `DIR` to see the
disk:

```
CompuPro Systems
64K CP/M 2.2K

A>DIR
```

The machine is the default machine of pascal100: a Z80 card, a CompuPro Interfacer 1 for the
console, a CompuPro Disk 1 floppy controller at `C0`, the host bridge at `B0` and 64K of RAM. After
a reset, the boot EPROM of the Disk 1 answers at `0000`. `RUN 0` is the whole startup of the
machine file: the EPROM reads track 0 of drive 0 and starts CP/M.

`cpm22-console.toml` is the same machine with the console on the terminal where you started the
program. Use it on a build with no window.

`^E` (STOP) gives the keyboard back to the monitor at any time. `RUN` continues the guest. `^C`
goes to CP/M, where it does a warm boot.

## Settings for one run

The machine files run CP/M at full speed. To change a setting for one run, give it on the command line with `--set`. The machine file does not change.
The text after `--set` is the text that you type after `SET` at the `pascal100>` prompt.

This command runs the Z80 at 2 MHz:

```
$ pascal100 cpm22.toml --set "cpu0 clock_hz=2000000"
```

This command makes the terminal window an ADM-3A, a period CP/M terminal:

```
$ pascal100 cpm22.toml --set "if1:a connect=terminal?emulation=adm3a"
```

You can give more than one `--set` in one command.

## The files

| File | What it is |
|---|---|
| `cpm22.toml` | The whole machine in one file: a Z80, an Interfacer 1, a Disk 1, the host bridge and 64K, the disk in drive 0 and `RUN 0`. The console is a **built-in VT100 window** that the simulator draws. The monitor stays in the terminal where you started the program. Needs a build with SDL3. |
| `cpm22-console.toml` | The same machine and disk, with the console on stdio. |
| `cpm22k-64k.dsk` | The system disk: CompuPro 64K CP/M 2.2K, BIOS at `F300`. |

The disk holds these programs:

- **Digital Research CP/M 2.2:** `ASM`, `DDT`, `DUMP`, `ED`, `LOAD`, `PIP`, `STAT`, `SUBMIT`,
  `SYSGEN`, `XSUB`, and the macro assembler `MAC` with `Z80.LIB`.
- **CompuPro:** `FORMAT` and `COPY` for the Disk 1.
- **The host bridge:** `R`, `W` and `HDIR` copy files between the host folder and the CP/M disk.
  The host folder is the folder where you started pascal100.
- **FujiNet:** `FUJIDIR`, `FUJIGET`, `FUJIPUT` and `WGET` (from fujinet-cpm22).
- **`LS`**, an extended directory program (v7a).

**There is no undo.** Drive 0 is read/write, as on a real machine, and CP/M writes to `A:` for each
file you make. In a clone, `git checkout` puts the image back. In a package, nothing does. Copy the
image before you test writes.

Drives 1 to 3 are empty. `MOUNT fd0:drive1 "my-scratch.dsk"` fills one.
