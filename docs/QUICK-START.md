# Quick start

**Boot CP/M with one command.** Type this in the folder where you unzipped the package:

```
$ ./pascal100 examples/cpm22/cpm22-console.toml
```

On **Windows**, the program is `pascal100.exe`, and the path uses backslashes:

```
> pascal100.exe examples\cpm\cpm22-console.toml
```

On **macOS**, start the program from a terminal. Do not double-click it: Finder opens a plain
program as a document. If macOS blocks the first start of a downloaded package, remove the
quarantine mark once:

```
$ xattr -dr com.apple.quarantine ./pascal100
```

The machine file boots the machine itself. A few seconds later, CP/M is ready:

```
CompuPro Systems
64K CP/M 2.2K

A>
```

Type `DIR` to see what is on the disk:

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

The disk has the Digital Research tools, the CompuPro `FORMAT` and `COPY`, and `R`, `W` and
`HDIR`, which copy files between CP/M and your computer.

## The three keys to remember

Two prompts share the window. `A>` is CP/M, and `pascal100>` is the monitor of the simulator.

| | |
|---|---|
| **`^E`** (Control-E) | Stop the machine and go to the `pascal100>` monitor. The machine loses nothing. |
| **`RUN`** | Start the machine again, from where it stopped. |
| **`QUIT`** | Leave the program. `Q` is enough. There is no `EXIT`. |

`^C` does not stop the machine. It belongs to CP/M, which does a warm boot. Use `^E`.

## The disk has no undo

The machine mounts the disk image read/write. Everything that you do in CP/M changes the file
on your computer.

- **To look without a change,** write-protect the disk at the monitor:
  `MOUNT fd0:drive0 cpm22k-64k.dsk WP`.
- **To write,** copy the folder first. The folder has all that the machine needs:

  ```
  $ cp -R examples/cpm22 my-cpm
  $ ./pascal100 my-cpm/cpm22-console.toml
  ```

Go back to the `A>` prompt before you quit. Then CP/M has written everything to the disk image.

## Where the files come from

- **The machine file that you name** is found from the folder that you are in.
- **Everything after that** is found from the folder of the machine file: the disk that it
  mounts, and each `MOUNT`, `LOAD`, `SAVE` and `DO` that you type at the monitor.

For this reason, each folder in `examples/` boots from any place that you copy it to.

A machine can also be set up by a script of monitor commands, with `-s`. The machines chapter
of the User Manual shows how.
`SHOW PATHS` at the monitor shows the folder. It also shows the one folder that `R` and `W`
can reach on your computer.

## The Pascal-100

The Pascal-100 machine is the file `examples/pascal/pascal.toml`. It is the Digicomp
Pascal-100: a Z80 and the Western Digital MicroEngine. It boots UCSD Pascal III.0. A system
disk comes with the package, ready to boot:

```
$ ./pascal100 examples/pascal/pascal.toml
```

A VT52 window opens with the Pascal `Command:` line. `examples/pascal/README.pdf` tells you
more.

## Where to go next

- `pascal100-manual.pdf`: the *User Manual*.
- `pascal100-monitor.pdf` and `pascal100-debugger.pdf`: the `pascal100>` prompt and the
  debugger.
- `examples/`: each folder has a `README.pdf` that tells you what to type.

The manuals are being rewritten for pascal100. Some parts still describe the Altair machines
of altairsim, the program that pascal100 comes from.
