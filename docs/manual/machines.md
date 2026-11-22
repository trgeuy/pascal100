# Machines

A **machine** is a backplane with boards in it. The boards, their settings and their state are
the whole machine. That is all that `pascal100` needs to know.

You choose the machine in one of three ways. You give the name of a **built-in** machine, you
give a **file**, or you give **nothing** and get the default. This chapter tells you how the
program makes that choice, and what a path means.

## The command line

```
pascal100 [machine] [options]
```

Give the machine first, and then the options for that machine. The program also accepts the
options before the machine: `pascal100 mine.toml -s check.cmd` and
`pascal100 -s check.cmd mine.toml` do the same thing.

| Option | |
|---|---|
| `machine` | a built-in name, **or** a machine file if it contains a `/` or ends in `.toml` |
| `-m, --machine <name>` | **always** a built-in name, never a file |
| `-f, --file <path>` | **always** a file, never a built-in name |
| `-n, --none` | an empty backplane. No boards, no memory, nothing |
| `-l, --list` | list the built-in machines and exit |
| `-s, --script <file>` | run a command script, then exit with its status. Paths in it are relative to the script's folder |
| `-x, --exec <cmd>` | run one monitor command, then exit. You can give it more than one time |
| `-i, --interactive` | after `--script` or `--exec`, stay in the monitor |
| `--set <setting>` | change a setting for this run, before the machine's startup commands. You can give it more than one time |
| `--mcp` | run as an MCP server on stdio. You cannot give `-s`, `-x`, `-i` or `--set` with it |
| `--mirror <sock>` | with `--mcp`, copy the console to a socket so that a person can watch |
| `-v, --version` | print the version and exit |
| `-h, --help` | print this help and exit |

**Give one machine only.** If you give a name *and* `-m`, or `-f` *and* `-n`, the program
prints *give ONE machine* and stops. It does not choose one for you.

## How a bare word resolves

```
$ pascal100 default                     a BUILT-IN, by name
$ pascal100 examples/cpm22/cpm22-console.toml             a FILE, by path
```

The program decides from the word only. **It never looks for a file on your disk:**

| The word | What it is |
|---|---|
| contains `/` or `\` | a **file** |
| ends in `.toml` | a **file** |
| anything else | a **built-in name** |

Because of this rule, **`pascal100 default` means the same machine in every folder.** A file
called `default` in your folder does not change it. A command in a script or in a README
always means the same thing.

For a file, you must give the `.toml` extension, as in `pascal100 mymachine.toml`, or a `/`,
as in `pascal100 ./mymachine`. To say which one you mean, use `-m` or `-f`:

```
$ pascal100 -m default                  a built-in. Stop looking for a file
$ pascal100 -f ./default                a file called `default`, no extension, right here
```

## The one file the simulator *finds*

If you give **nothing**, and the working folder has a file called `pascal100.toml`, the program
loads that machine. It tells you that it did:

```
$ pascal100
pascal100: no machine named -- using ./pascal100.toml (`-m default` for the built-in).
pascal100 X.Y.Z (commit) -- z80, full speed.
machine: bench.  HELP for commands.
pascal100>
```

The first line tells you which file the program loaded. The program prints it before anything
else. The `machine:` line gives the name that is written *in* the file, `bench` here.

This is the **only** file that the program finds by itself. It does this only when the command
line gives no machine. If you give a built-in name, a file or `-n`, the program ignores
`./pascal100.toml`.

Put a `pascal100.toml` in a project folder, and `pascal100` with no arguments loads your
machine. With no `pascal100.toml` and no arguments, you get the built-in `default` machine.

## The built-in machine

**One machine is stored in the program: `default`.** It is a TOML machine file, in the same
format, keys and rules as a machine file that you write. It is in the program so that it is
always there: a Z80, an Interfacer 1 console, a Disk 1, the host bridge and 64K, with no disk.

```
$ pascal100 --list
```

This command lists the built-in machine with a sentence about it. The machine reference at the
back of this manual has the same table. At the `pascal100>` prompt, `SHOW MACHINES` gives the
list.

**The other machines are files** in `examples/`. Each one is complete: every board is in the
file, so you can read from top to bottom how the machine is built. To start your own machine,
copy the example that is nearest to what you want and edit it.

To see what is in a machine, its boards and its startup commands:

```
$ pascal100 default -x 'SHOW MACHINE'
```

At the prompt, type `SHOW MACHINE default`. `SHOW MACHINE` with no name shows the machine that
you are running now.

To get the machine that you are running as **text that you can edit**, with every board and
every setting:

```
$ pascal100 default -x 'CONFIG SAVE mine.toml'
$ pascal100 mine.toml
```

`CONFIG SAVE` saves the machine that you are running, and the file loads back as the same
machine. It writes every setting, also the ones at their default value, so the file is long.
An example file is shorter, because it gives only what is not a default.

You can also start *from* another machine with `base`, and write only what is different.
The configuring chapter tells you how. A complete file is easier to read, so the examples do
not use `base`.

## How the machine is wired: `SHOW BUS`

`SHOW MACHINE` tells you which boards are in the machine. `SHOW BUS` tells you how they are
wired together. It shows what answers at each memory address, what answers at each port, and
what happens on an interrupt. Use it first on a machine that you built. For example, type
`pascal100 -x 'SHOW BUS'` (the `default` machine), or type `SHOW BUS` at the prompt:

```
pascal100> SHOW BUS
MEMORY
  000000-00FFFF  fd0      boot ROM every memory read gets byte A0-A7 of one 256-byte routine, over PHANTOM* until OUT BASE+3 with D0 = 0  all segments
  000000-00FFFF  mem0     ram  ext  16-bit
  other segments: only the boards marked "all segments" answer
I/O
  00             if1      IN  OUT    Channel A -- receive (R) / transmit (W)
  01             if1      IN  OUT    Channel A -- status (R) / control latch (W)
  02             if1      IN  OUT    Channel B -- receive (R) / transmit (W)
  03             if1      IN  OUT    Channel B -- status (R) / control latch (W)
  C0             fd0      IN  --     765 main status
  C1             fd0      IN  OUT    765 data
  C2             fd0      IN  OUT    board status, D7 = INT (in) / DMA address, 3 bytes MSB first (out)
  C3             fd0      IN  OUT    D7 = serial in (in) / serial out (out), 1 = SPACING; D0 = 0 switches the boot ROM off
  B0             hb0      IN  OUT    Host Bridge: command / status
  B1             hb0      IN  OUT    Host Bridge: data -- file names, file bytes, directory names
INTERRUPTS
  CPU     INTE off         interrupts are DISABLED; nothing will be acknowledged
  pINT    idle             pin 73
  VI      (none)           nothing watches VI0-VI7; an acknowledged interrupt
                           floats to FF, which the processor executes as RST 7

DMA
  pHOLD   idle             pin 74
  TMA 15  fd0      disk1    set by dma_priority
```

The output has three sections:

- **`MEMORY`** lists each range and the board that answers it. A range with no board is
  `unmapped`. This is not an error: a read from it returns `FF`, the same as an empty socket on
  a real backplane. A boot ROM that the program has switched out is not in the list. On the IEEE
  696 bus, each range also says which 64K segments it answers.
- **`I/O`** lists each port and the board that answers it. The `IN` and `OUT` columns show which
  direction the board answers. `--` means that the board does not answer that direction. A port
  that is not in the list returns `FF` on an `IN`.
- **`INTERRUPTS`** is the short form of `SHOW BUS IRQ`. It shows whether the processor accepts
  interrupts, whether the interrupt pin is asserted, and whether a board, such as the System
  Support 1, is there to read lines VI0 to VI7. It lists each line that has a board set to it. A board that is set to a
  line that nothing reads does not give an error. The machine runs, and it never gets the
  interrupt. A `WARNINGS` part shows you this fault.
- **`DMA`** lists each board that can take the bus (a DMA board) and its priority, highest
  first. It shows only when the machine has a DMA board. Two DMA boards with the same priority
  give a warning, and the machine does not run until you change one `dma_priority`.

To see only one section, or more detail, add a view: `SHOW BUS MAP`, `SHOW BUS IO`,
`SHOW BUS IRQ` or `SHOW BUS DMA`. `SHOW BUS CONTENTION` shows two boards that answer the same address or port. The
boards chapter has an example. The Monitor document has the full list of views.

## The empty backplane: `-n`

```
$ pascal100 -n
```

`-n` gives a machine with no boards, no memory and no processor. You add every board yourself
with `BOARDS ADD`. Use it when you build a machine one board at a time. It is also the only
way to be sure that the machine has nothing in it that you did not add.

### From an empty backplane to a machine file

These steps build a machine at the prompt, save it to a file, and load it again:

```
$ pascal100 -n
pascal100> BOARDS ADD z80 cpu0
cpu0: z80 added
pascal100> BOARDS ADD io4 io40
io40: io4 added
pascal100> SET io40:a connect=console
io40:a: connect=console
pascal100> BOARDS ADD memory mem0 fill=random
mem0: memory added
pascal100> REGION ADD mem0 type=ram at=0 size=56K
mem0:0: ram  0000-DFFF  56K
pascal100> REGION ADD mem0 type=rom at=F000 mount=builtin:ssm-8080mon
mem0:1: rom  F000-F7FF  builtin:ssm-8080mon
pascal100> CONFIG SAVE mine.toml
saved mine.toml
pascal100> QUIT
```

To get the machine back, start with an empty backplane again and load the file:

```
$ pascal100 -n
pascal100> CONFIG LOAD mine.toml
loaded mine.toml: 3 board(s)
```

`$ pascal100 mine.toml` does the same thing in one step.

Three things in these steps are important:

- **You can give a board's settings on the `BOARDS ADD` line** (`fill=random`), or set them
  later with `SET`. The result is the same.
- **`SET` sets one property in each command**, and the board must exist first.
- **A memory board has no memory until you add regions to it.** Use `REGION ADD`. Do not forget
  the ROM region, because it holds the code that `RUN` starts.

Two more commands are often part of a machine. `CONFIG SAVE` saves both of them:

- **`STARTUP ADD RUN F000`** records the command that starts the machine. Here it starts the SSM
  monitor in the ROM, which prints `MONITOR V1.0` on the console. Use `MOUNT` to put a disk in a
  drive.
- **`SET MACHINE name=<name>`** gives the machine a name. A machine that you build from `-n` is
  called `none` until you give it a name. `CONFIG SAVE` saves the name. The name is a label:
  the `base =` key of another file finds this machine by the path of its file, not by its name.

## The path rule: one base directory

This rule lets you copy an example folder to any place, and the machine still boots:

> **A relative path resolves against the machine's directory**, which is the folder that the
> machine file was loaded from.

That folder is the base for the disks and PROMs that the machine file mounts. It is also the
base for the `MOUNT`, `LOAD`, `SAVE` and `DO` paths that you type. The rule is the same for a
path in the file and a path that you type.

A **script** follows the same idea. A path in a script is relative to the script's folder, as a
path in a machine file is relative to the machine file. This is true for a script that you run
with `DO` and for a script that you run with `-s`.

`examples/cpm22/cpm22-console.toml` has `mount = "cpm22k-64k.dsk"`. This means *the disk in
this folder*. It still means that after you copy the folder, rename it or send it to another
person. For this reason, each example is a folder that has all its files, and the `cp -R` in
the quick start works. If you then type this:

```
pascal100> MOUNT fd0:drive1 cpm22k-64k.dsk
```

you get the **same file**, from the **same folder**, the folder of the machine file. The folder
that you started `pascal100` from does not change this.

A **built-in** machine has no folder of its own. Its base is the folder that you started the
program from.

### When the rule causes a problem

You see the rule only when a file is missing. When that happens, the error can look like a
typing mistake. For example, you keep your machine files in a `machines/` folder. You write a
path that starts from the folder that you start the program from:

```toml
[[board.drive]]
unit  = 0
mount = "disks/cpm/cpm.dsk"         # meant: the disks/ up beside machines/
```

`pascal100 -f ./machines/bench.toml` then prints the error, with the line of the
`[[board.drive]]` in your file:

```
./machines/bench.toml: line 12: fd0: 'machines/disks/cpm/cpm.dsk': no such file
  ('disks/cpm/cpm.dsk' is relative to the machine's directory, ./machines/)
```

**The disk is not missing.** The program looked for it in the folder of the machine file,
because that is the machine's directory. Write the path from that folder:

```toml
mount = "../disks/cpm/cpm.dsk"      # up out of machines/, then down into disks/
```

You can also keep the machine file in the same folder as its disks, as every example in the
package does. The same `../` applies to a path that you type, because both start from the same
base.

### Ask the machine

`SHOW PATHS` prints the base for the machine that you are running:

```
pascal100> SHOW PATHS
  base directory     /home/you/pascal100/examples/cpm22
                     What a machine file mounts, and the MOUNT / LOAD / SAVE /
                     DO you type, resolve against this. A path inside a DO or
                     -s file is relative to that file.
                     It is the directory the machine was loaded from.

  hb0 sandbox        /home/you/pascal100/examples/cpm22
                     THE GUEST'S SANDBOX, and the only real fence here:
                     R.COM/W.COM cannot leave it. It is not a base for
                     anything you type. Set with `hostdir`.
```

The two entries do different jobs. The base directory is where relative paths start. The
sandbox is the only folder that the guest can read and write.

For a **built-in** machine, the base is the folder that you started the program from:

```
  base directory     /home/you/pascal100
                     ...
                     This machine is built in, so it is the directory you
                     launched from.
```

`SHOW MOUNTS` lists every disk and ROM that you can mount in the machine, for all the boards:

```
pascal100> SHOW MOUNTS
  UNIT        KIND  HOLDS
  fd0:drive0  disk  cpm22k-64k.dsk
  fd0:drive1  disk  (empty)
  fd0:drive2  disk  (empty)
  fd0:drive3  disk  (empty)

  Paths are AS WRITTEN.  SHOW PATHS says what they are relative to.
```

**Empty drives are in the list.** The Disk 1 has four drives. This machine has a disk in one of
them.

`SHOW MOUNTS` shows each path as it was written. `SHOW PATHS` tells you which folder those paths
are relative to. Use the two commands together.

### The path rule is not a sandbox

The path rule tells the program **where a path points**. It does not limit anything. A machine
file can mount any file on your disk, with `..` or with an absolute path.

The only limit is the **`hostdir`** of the Host Bridge. It limits the files that a CP/M program
*in* the machine can read and write on your computer. This is a different mechanism for a
different purpose. The chapter *Moving files in and out* describes it. Nothing in a machine
file's paths changes it.

## Changing settings for one run: `--set`

A machine file gives each setting one value. To run the machine with a different value, give
the setting on the command line with `--set`. The machine file does not change, and you do not
need a second machine file.

For example, `cpm22-console.toml` runs CP/M at full speed. This command runs the same machine
at 2 MHz:

```
$ cd examples/cpm22
$ pascal100 cpm22-console.toml --set "cpu0 clock_hz=2000000"
pascal100 X.Y.Z (commit) -- z80, full speed.
machine: cpm22-console.  HELP for commands.
set> SET cpu0 clock_hz=2000000
cpu0: clock_hz=2000000
startup> RUN 0
[console -- ^E returns to the monitor]

CompuPro Systems
64K CP/M 2.2K

A>
```

The first two lines show the machine that the file made, which runs at full speed. The `set>`
line shows the change. Without `--set`, the machine runs at full speed again:

```
$ cd examples/cpm22
$ pascal100 cpm22-console.toml
```

**The text after `--set` is what you type after `SET` at the monitor.** `SET cpu0
clock_hz=2000000` at the monitor becomes `--set "cpu0 clock_hz=2000000"` on the command line.
The text has a space in it, so put it in quotation marks. The keys are the keys of the machine
file. `SHOW cpu0` lists the keys of a board, and the boards chapter describes them.

**The program applies each setting before the machine's startup commands.** The order is:

1. The program loads the machine file, and shows what it loaded.
2. It runs `~/pascal100.ini`, if you have one. A later section describes this file.
3. It applies each `--set`, in the order that you gave them, and shows each one behind `set>`.
4. It runs the `startup` commands of the machine file.

A `--set` comes after your `~/pascal100.ini`. If the two set the same key, the `--set` is the
value that the machine has.

This order is why `--set` works for a machine that starts by itself. `cpm22-console.toml` has
`startup = ["RUN 0"]`, so CP/M starts as soon as the file is loaded. A `-x "SET ..."` runs
after the startup commands, when CP/M has started. `--set` runs before them.

**You can give `--set` more than one time.** This command puts the CP/M console in a terminal
window of its own:

```
$ cd examples/cpm22
$ pascal100 cpm22-console.toml --set "if1:a connect=terminal" --set "TERMINAL bsdel=bs"
```

`if1:a` is channel `a` of the Interfacer 1, which is the console of this machine. `connect` is
the device on that line, and `terminal` is a VT100 in a window. `TERMINAL bsdel=bs` makes the
Backspace key in the window send the byte that CP/M expects. CP/M starts in the window, and the
monitor stays in the terminal where you started the program. The serial chapter describes
`terminal` and its options.

To run that machine at 2 MHz also, add `--set "cpu0 clock_hz=2000000"` to the same command.

- **The machine file does not change.** `--set` is for this run only.
- **A setting that fails stops the program.** If the board, the key or the value is wrong, the
  program shows the error from `SET`, then `--set failed; the machine was not started.` The
  exit status is 2. The startup commands do not run.
- **You can use `--set` with `-x`, `-s` and `-i`.** The program applies the settings before it
  runs the commands.
- **You cannot use `--set` with `--mcp`.** An MCP client sends `SET` with the `monitor` tool.

## Running a command and leaving: `-x` and `-s`

You can use `pascal100` without typing at the prompt.

```
$ pascal100 default -x 'SHOW MACHINE'
$ pascal100 examples/cpm22/cpm22-console.toml -x 'DUMP 0 F'
```

`-x` runs one monitor command on the machine, and then the program exits. You can give `-x` more
than one time. The commands run in the order that you give them:

```
$ pascal100 examples/cpm22/cpm22-console.toml -x 'MOUNT fd0:drive1 mine.dsk' -x 'RUN 0' -i
```

Without `-i`, the program exits when the commands are done. With `-i`, you get the monitor, and
the machine is as your commands left it. `-i` with no `-x` or `-s` has no effect.

`-s` runs a **script**. A script is a file of monitor commands, one on each line. These are the
same commands that you type at the prompt. For example, put these three lines in a file called
`boot.cmd` in `examples/cpm22`. They build the same CP/M machine as `cpm22-console.toml`, and boot
it:

```
MACHINE default
MOUNT fd0:drive0 "cpm22k-64k.dsk"
RUN 0
```

```
$ cd examples/cpm22
$ pascal100 -s boot.cmd
```

At the `pascal100>` prompt, `DO boot.cmd` runs the same script.

**A path in a script is relative to the script's folder.** The script above mounts
`cpm22k-64k.dsk`, the disk beside it. So the script also works from another folder. Give the
path to the script from the folder that you are in:

```
$ pascal100 -s examples/cpm22/boot.cmd
```

**The exit status is not zero if a command failed.** You can use `pascal100 -s` in a shell
script, a Makefile or a build, and test the result:

```sh
if pascal100 mine.toml -s check.cmd; then
    echo "machine is sane"
fi
```

This command has one script and one machine. The program loads the machine `mine.toml`, and
runs its startup commands. It then runs each line of `check.cmd` on that machine. A script
takes no arguments.

## A script that runs at every start: `~/pascal100.ini`

If there is a file called `pascal100.ini` in your home folder, `pascal100` runs it each time
that you start the monitor. It is a script, the same kind as `-s` runs: monitor commands, one on
each line. Use it for settings that belong to you and not to a machine, for example where the
debug output goes. This line writes it to a file in your home folder:

```
# ~/pascal100.ini
SET CONSOLE DEBUG=pascal100-debug.log
```

The file runs after the machine is loaded and before the machine's own `startup` commands, so a
machine that starts running at once still gets your settings. A `--set` on the command line is
applied after the file runs, and before the `startup` commands. The program shows each line
behind `ini>` as it runs it.

- **No file is not an error.** Without the file, nothing happens.
- **A command that fails does not stop the file.** The program shows the error, runs the next
  line, and the exit status is not changed.
- **A path in the file is relative to your home folder.**
- **`--mcp`, `-x` and `-s` do not run it.** Their output is read by a program, and a file in
  your home folder must not change it. To use the same lines in a script, name the file:
  `pascal100 -s ~/pascal100.ini`.

## Which chapter next

The **configuring** chapter describes the machine file: every table, every key, and the four
forms of a `[[board]]` entry. The **boards** chapter describes each board.
