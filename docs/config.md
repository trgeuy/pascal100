# Machine configuration (TOML)

A machine is described by a TOML file. The **same state** is reachable at runtime through the monitor, and `CONFIG SAVE` saves it back out.

**The TOML keys for a board *are* its `properties()`** (see `DESIGN.md` §5) — there is no separate config schema per board. The loader and `CONFIG SAVE` are the same code path, so they cannot drift, and a board added next year is configurable the day it lands with no changes here.

## Which machine you get

| You type | You get |
|---|---|
| `pascal100 default` | the built-in `default` — **in every directory on earth** |
| `pascal100 cfg/mine.toml`, `pascal100 -f mine` | that file |
| `pascal100 -m default` | the built-in, always |
| `pascal100 -n` | an empty backplane |
| `pascal100` | **`./pascal100.toml` if the working directory has one**, else the built-in `default` |

**`./pascal100.toml` is the only file the simulator *finds* rather than is *given*, and it is found only when the command line names nothing at all.** That restriction is the whole licence for the feature. `looksLikeFile()` (`src/core/machines.h`) refuses to probe the disk on purpose — `pascal100 default` must not become a different machine the day somebody saves a file called `default` next to it, because a command line that changes meaning because of its surroundings is a trap. That argument applies to every command that *names* a machine. A bare `pascal100` names none: it is not asking for `default`, it is asking for whatever machine is sensible *here*, and letting the directory answer is the `make(1)` bargain rather than the trap.

**It is never silent.** When the working directory's file is used, the simulator says so on **stderr** — because the failure this can otherwise cause is spending twenty minutes on a machine you did not know you were running. It goes to stderr and not stdout so that a `-s` script's output stays exactly what the script printed.

A found file is an *ordinary* config file, so it can `base` off a built-in and say only what is different:

```toml
# ./pascal100.toml -- the project's machine. Just `pascal100` boots it.
[machine]
name = "myproject"
base = "default"

[[board]]
id = "fd0"              # the default's Disk 1 (no type: modify it in place)...

  [[board.drive]]
  unit  = 0
  mount = "disks/cpm.dsk" # ...with this project's disk in drive 0
```

## Where a relative path is relative to

There is one answer, and it is the whole of the rule.

> **A relative path resolves against the machine's directory** — the folder the machine file
> was loaded from.

That covers everything: `mount`, `base`, the `MOUNT`/`LOAD` commands inside a `startup` list, a
`-s` script, and anything you type at the prompt. One base, so a disk the machine ships with and
a disk you mount by hand are found in the same place. (A **built-in** machine has no directory
of its own, so its base is the directory you launched from — the only anchor it has.)

That a machine file's own paths point at its own directory is what makes it portable.
`examples/cpm22/cpm22-console.toml` says:

```toml
[[board]]
id = "fd0"

  [[board.drive]]
  unit  = 0
  mount = "cpm22k-64k.dsk"     # the disk lying beside this file
```

…and means the disk in its own directory. So both of these work, and mean the same machine:

```sh
cd examples/cpm22 && pascal100 cpm22-console.toml  # the way you will actually run it
pascal100 examples/cpm22/cpm22-console.toml         # ...and from anywhere else
```

That matters because **a machine file ships with its media beside it.** A user gets the binary and the `examples/` tree, each example a folder holding a machine file and the disk it mounts — not this repository. A machine file that only resolved from the repository root would be a machine file that only worked for us, and every example in this tree used to be exactly that.

The same base covers what you type. When you type

```
pascal100> MOUNT fd0:drive1 "scratch.dsk"
```

you get the `scratch.dsk` in the machine's own directory — the same folder its own disks come from — no matter which shell you launched from. Typed paths used to resolve against your shell instead, which is how the identical disk could appear under two different names; `acceptance-media` has a control that fails the build if a typed path ever stops resolving against the machine's directory.

**There is no search path.** A file is looked for in exactly one place. If it is not there, the error names the place it looked — not the name you wrote — because the whole point of a resolved path is to be able to see where it went.

**What is stored is what you wrote.** `SHOW` prints, and `CONFIG SAVE` saves, the path *as it appears in the file* — so a machine saved out of `examples/cpm22/` still says `mount = "cpm22k-64k.dsk"` and still loads from its own directory. Only the *narration* ("`mounted …`", "`loaded …`") names where the file actually was, because that is a report of what happened rather than a record of what was asked for.

Absolute paths and the `builtin:` scheme are never re-based: `mount = "builtin:disk1"` is a ROM in the binary, not a file, and must never become one.

### This is not a sandbox

**Nothing on this page confines anything.** The rule answers one question — *what is a relative path relative to* — and that is a lookup, not a fence. A machine file may write `../../../etc/passwd` and it will be opened. `MOUNT` has never been restricted to a directory tree, and none of this is a security boundary.

**The Host Bridge's `hostdir` is the other thing, and it is a real fence.** That one is a sandbox: it is the directory a *guest* program (`R.COM`, `W.COM`) is confined to, and it refuses `..` components, absolute paths, drive letters, and symlink escapes. It defaults to the directory you launched `pascal100` from. See `docs/boards/hostbridge.md`.

The two are independent and share no mechanism. They get confused because both end in the words "which directory?" — but one decides where a path **points**, and the other decides how far a guest may **reach**. Moving one would not move the other.

## Errors name the line, and a table is written once

Every error from the loader reads `<file>: line N: <what is wrong>`. `N` is the line of the key at fault, or of the table header when the fault is the table (a sub-unit table that its board refused, an unknown table, a `[[board]]` with no `id`). A fault inside a `base` file names both links: `mine.toml: line 3: base.toml: line 9: ...`.

**`[machine]`, `[console]`, `[display]` and `[terminal]` appear once per file; `[board.unit.<name>]` once per `[[board]]`; a key once per table.** A second one is refused and the error gives both lines — it is almost always two files pasted together, and before this check the second `[machine]` was reported as whatever its `base` tripped over. The scope is the *file*: a `base` is a separate file, so restating a table the base wrote is the ordinary delta. Two `[[board]]` blocks that MODIFY the same id stay legal. A key above the first table is an error, as is text after a table header and a `startup = [` that never closes.

## Verbs

- **`MOUNT`** refers to **host files** (disk and ROM images).
- **`CONNECT`** refers to **sockets and serial ports** (and the console).

In TOML these appear as the `mount` and `connect` keys; at the monitor they are the `MOUNT` and `CONNECT` commands. They mean the same thing.

## Endpoints (for `connect`)

**Not listed here.** The endpoints are specified once, in the User Manual's serial chapter
(`docs/manual/serial.md`), and `HELP CONNECT` prints the grammar from the same function the
resolver uses. A copy here would be a second list, and the one that used to be here had
already drifted: it named a `file:` endpoint that never existed and left out `telnet:`,
`in:`/`out:`, `terminal`, `printer:` and the taps. The *why* for the ones that need arguing is
in `docs/cli-commands.md` (*The endpoints*).

## Example — an annotated machine

This is `tests/media/altmon/altmon.toml`, the machine the 2SIO tests boot, with the keys
spelled out. It loads and boots ALTMON.

```toml
[machine]
name    = "altmon"
startup = ["RUN F800"]     # anything you can type, a config can do

# The CPU is a board like any other (DESIGN.md §3) -- and THE CRYSTAL IS ON IT,
# which is why clock_hz is this board's property and NOT the machine's.
[[board]]
type     = "z80"
id       = "cpu0"
clock_hz = 0               # 0 = run flat out, AND IT IS THE DEFAULT.

# Two 6850 ACIAs on one card. They share NOTHING -- separate baud jumpers,
# separate endpoints, separate interrupt straps -- so almost everything here is a
# UNIT property, not a board one.
[[board]]
type = "2sio"
id   = "sio0"
port = 10                  # HEX: a port is on the wire. Occupies 10-13.

  [board.unit.a]
  connect = "console"
  baud    = 9600           # DECIMAL: a baud rate never is.

  [board.unit.b]
  connect = "null"         # NOT an error: an unconnected 6850 sits there with TDRE
  baud    = 9600           #   set forever and talks to nobody, exactly as the card
                           #   does with nothing in the second socket.

[[board]]
type           = "memory"
id             = "mem0"
fill           = "random"  # real static RAM does not come up zeroed
honors_phantom = "read"
ext            = true      # IEEE 696: decode A16-A23; `at` is a 24-bit address
sixteen_bit    = true

  [[board.region]]         # A card carries one or more POPULATED regions.
  type = "ram"             #   ram = writes are stored.  rom = writes are not decoded.
  at   = 0000
  size = "62K"

  [[board.region]]
  type  = "rom"
  at    = F800
  mount = "ALTMON.HEX"     # the file beside this machine file
```

**Keep `[machine]` keys above the first `[[board]]`.** This is TOML scoping, not a rule of ours:
once a `[[board]]` (or any `[table]`) block opens, every bare key that follows belongs to *it*. A
`startup = [...]` written **after** a board block is parsed as a property of that board and
rejected as an unknown board key — the machine's own `startup` silently never gets set. Put
`name` and `startup` directly under `[machine]`, before you fit the first board, as above.

### The transform chain is the CONSOLE's, and only the console's

`UPPER`, `STRIP7OUT`, `CRLF`, `BSDEL` and the rest belong to `[console]` — **not** to a board and **not** to a unit. Every other endpoint (socket, serial port, file) is **8-bit clean, always**, because the next thing down that line may be XMODEM and a filter would corrupt it silently (DESIGN.md §7.2).

```toml
[console]
upper     = true         # fold keyboard input to uppercase
strip7in  = true
strip7out = true         # a Teletype ignores bit 8; some programs set it as a terminator
bsdel     = "bs"         # off | bs (fold DEL->BS) | del. A perennial CP/M annoyance.
crlf      = false        # usually WRONG to turn on: period software sends its own LF
echo      = false        # local echo, for half-duplex hardware
bell      = true
```

What a **board** gets instead is **line coding** — and only where it is really a jumper or a
strap:

```toml
[[board]]
id = "if1"

  [board.unit.b]
  data_bits = 7          # a STRAP on the Interfacer 1. A FRAME, not a mask.
  parity    = "on"
  stop_bits = 2
```

On a real serial port those are programmed into the real port. The Interfacer 1's guest can
invert each strap through its control latch, and on the **88-2SIO** there is no such property
at all: the 6850's word format is a register the *guest* writes.

At the monitor that is `SET CONSOLE UPPER=ON`, and `SHOW CONSOLE` prints it. **It is not `SET if1:a UPPER=ON`** — a transform is the console's and nothing else's, because a card's line has to stay 8-bit clean or a filter on it corrupts XMODEM, silently.

`[console]` holds only what is genuinely about a *terminal*:

```toml
[console]
stop = 05                  # HEX. The STOP key (^E): it takes a RUN back to the monitor.
                           #   The guest NEVER SEES this byte, so it cannot disable it.
                           #   `attn` is the old spelling, and still accepted.
history = 100              # command lines kept in .pascal100_history (per launch dir).
                           #   Default 50; 0 turns the file off.
```

`rows`, `cols`, `pace`, `ansi` and `tabs` are specified in DESIGN.md §7.2 but **not built yet**.

### A second 2SIO — same type, own config, different base port

```toml
[[board]]
type = "2sio"
id   = "sio1"
port = 14
```

## Example — a CP/M machine

This is the `default` machine, as `CONFIG SAVE` writes it, with a disk and a `startup` added.
`examples/cpm22/cpm22-console.toml` is the same machine, written out with only the settings that are not defaults.

```toml
[machine]
name    = "cpm-dev"

startup = [                # monitor commands, run in order once the boards exist.
  "RUN 0",                 #   start the Disk 1 boot EPROM. There is no BOOT command
]                          #   (DESIGN.md §10.0) — this is the operator's keystroke,
                           #   written down. Anything you can type, a config can do.

# The CPU is a card, and THE CRYSTAL IS ON IT -- which is why clock_hz is this
# board's property and not the machine's.
[[board]]
type     = "z80"
id       = "cpu0"
clock_hz = 0
idle     = true          # ...and so is the OTHER sleeping policy, and it is the DEFAULT.
                         # A guest at a prompt does nothing but poll an empty keyboard,
                         # and running that flat out cost a whole host core. The guest
                         # cannot tell -- no board behaves differently and emulated time
                         # is untouched; only the HOST sleeps. `idle = false` gets the
                         # spin back.

# Two serial channels, each a 1602-family UART. Channel a is the console.
[[board]]
type = "interfacer1"
id   = "if1"

  [board.unit.a]
  connect = "console"

# The floppy controller, and THE BOOT EPROM IS ON IT. The EPROM answers at 0000 over
# PHANTOM* until the guest writes OUT C3 with D0 = 0, so RUN 0 is a cold start.
[[board]]
type    = "disk1"
id      = "fd0"
port    = 0xC0             # occupies C0-C3
routine = 4                # which 256-byte routine of the EPROM answers. 4-7 are the
                           #   8080/Z80 routines, 0-3 the 8086 ones.

  [[board.drive]]
  unit     = 0
  mount    = "cpm22k-64k.dsk"
  readonly = false         # true = write-protect: the 765 reports it to the guest.

# One card, one region: 64K of RAM, under the EPROM.
[[board]]
type           = "memory"
id             = "mem0"
honors_phantom = "read"    # A JUMPER: while the EPROM pulls PHANTOM*, reads come from
                           #   the EPROM and writes still reach this RAM.
ext            = true
sixteen_bit    = true

  [[board.region]]
  type = "ram"
  at   = 0x0000
  size = "64K"

# GUEST <-> HOST FILE TRANSFER, and it is OUR OWN CARD -- no S-100 maker made it.
# It is in the DEFAULT machine, so you usually inherit it rather than write this.
[[board]]
type    = "hostbridge"
id      = "hb0"
port    = B0               # 2 ports, B0-B1
hostdir = "./hostfiles"    # THE SANDBOX ROOT. Guest names cannot escape it: no `..`
                           # component, no absolute path, no drive letter, no symlink
                           # that resolves out. Subdirectories inside it are fine, and
                           # both `/` and `\` work on every host.
                           #
                           # EMPTY (the default) MEANS THE DIRECTORY YOU RAN pascal100
                           # FROM -- which is what makes `R FOO.ASM` work on the file
                           # you are looking at with nothing configured. (A relative
                           # hostdir you write still follows the path rule above.)
readonly = false           # ON makes it a one-way street: out of the host only.
```

Then, at the CP/M prompt (`cpm/hostbridge/*.ASM`, assembled in the machine with
`ASM`/`LOAD`):

```
HDIR              what is on the host
R  FOO.ASM        host -> CP/M          R *.ASM      R SRC/FOO.ASM
W  FOO.ASM        CP/M -> host          W *.HEX      W FOO.DAT T   (T = stop at ^Z)
```

## The reference moved — and this file did not

**The normative TOML reference now lives in the User Manual** (`docs/manual/configuring.md`),
which is the document that ships to users and is therefore the document that has to be
complete. Every `[machine]` key, all four `[[board]]` forms, the region and drive tables, the
number rule, `[console]` — they are specified there, once.

They used to be specified *here as well*, and that was the bug. Two normative descriptions of
one format is precisely the "second schema" this project refuses everywhere else in the code,
committed in the docs instead — and the copy here had already drifted (it claimed a machine
could have "config-time properties" that are "rejected on a running machine", a rule that was
deleted in July 2026 and never existed in the code by the time anybody read the sentence).

**What stays here is the part that is not a specification: the arguments.** The worked examples
above, and the reasoning in them — why `clock_hz` is the CPU card's property and not the
machine's, why the transform chain belongs to the console and to nothing else, why a second 2SIO needs nothing but a different base port. That is
what a design document is for, and none of it belongs in a user manual.

If you are looking for **what a key does**, read the manual. If you are looking for **why the
format is shaped like this**, you are in the right place.

## And the per-board keys are not written down anywhere by hand

A board's `properties()` **are** its TOML keys. There is no separate schema, and there is no
hand-maintained table of them — `docs/manual/ref/boards.md` is *printed from the binary* by
`tools/gen-reference.cpp`, and a test fails if it is stale.

That is not a convenience. The first table of the memory card's defaults that anybody typed out
by hand, reading the source carefully, got three of eight rows wrong.
