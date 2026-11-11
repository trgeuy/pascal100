# The monitor

This document is one of two short documents about how to use **pascal100 itself**. pascal100 is the
program that you run, and the machines that it simulates are a different subject. This document
is about **the monitor**. The monitor is the `pascal100>` prompt, where you start, stop, examine
and change the machine. The other document, *The Debugger*, tells you what to do in the monitor
when a guest does not work correctly.

Both documents are companions to the *User Manual*. The *User Manual* describes the hardware: the
boards, the machines and the disks. pascal100 simulates the Digicomp Pascal-100 and the IEEE 696
(S-100) boards around it. If you do not know these, start with the first chapter of the *User
Manual*.

The `pascal100>` prompt is **the monitor**. The monitor is the front panel of the machine, and
it is also the debugger. In pascal100, the front panel and the debugger are one thing. The monitor
can do all that the front panel of an S-100 machine could do, and much more. It sets breakpoints,
single-steps and disassembles. It also shows you the bus: which board decodes each address, which
board asserts each interrupt line, and where two boards decode the same address. *The Debugger*
describes these debug tools.

The monitor operates the machine directly. It is not a *menu* that stands between you and the
machine and lets you examine a fixed set of things. There is no debug mode to enter, and no
process watches the machine from outside. A breakpoint is **the machine stopping**. It is not a
script that sees afterward that the machine should have stopped.

`IN` and `OUT` do **real bus cycles**, with all the side effects of a real cycle. If you read a
UART's data port in the monitor, you take the byte from the UART, as the guest would. The front
panel and the debugger are one object because on a machine with a front panel they were one
object. The operator set the switches and read the lamps.

The monitor works while the machine is stopped. Most of the tasks that follow work with the
power on and the processor stopped. Examples are examining memory, doing a bus cycle and adding
a board. The front panel was also made for a machine with the power on and the processor stopped.

## Commands resolve by prefix

The monitor finds a command from the first letters that you type. It has **no aliases and no
abbreviations to learn**. Type enough letters to make the command unambiguous. The first command
that matches is the one that runs.

`HELP` prints the full list of commands, with the shortest form of each command outside the
brackets:

```
  BO[ARDS]          B[REAK]           COM[PARE]         C[ONFIG]
  CONN[ECT]         CONS[OLE]         DE[POSIT]         DI[SASM]
  DISC[ONNECT]      DO                D[UMP]            E[DIT]
  EX[AMINE]         F[ILL]            HE[LP]            H[ISTORY]
  I[N]              L[OAD]            MA[CHINE]         M[OUNT]
  MOV[E]            N[EXT]            NO[BREAK]         NOP[ASTE]
  O[UT]             PA[STE]           P[OWER]           Q[UIT]
  REGI[ON]          RE[GS]            RES[ET]           REST[ORE]
  R[UN]             SA[VE]            SEA[RCH]          SE[T]
  SH[OW]            SN[APSHOT]        STA[RTUP]         S[TEP]
  SY[MBOLS]         T[RACE]           TY[PE]            U[NMOUNT]
  W[HO]
```

Type the part before the bracket. `D` is `DUMP`, `DE` is `DEPOSIT` and `RES` is `RESET`. Upper
case and lower case are the same, in a command and in the name of a board.

`HELP <command>` shows the usage and examples for one command. `?` is the same as `HELP`.

Some commands are shortcuts for a longer command. The help of a shortcut says which command
it is short for. `CONSOLE` is a shortcut for `SET CONSOLE` and `SHOW CONSOLE`. Thus
`HELP CONSOLE` prints the full help of `SET CONSOLE`, which is also what `HELP SET CONSOLE`
prints.

> **`R` is `RUN`, not `RESET`. To reset, type `RES`.** You type `RUN` in every session, and an
> unwanted `RUN` does no damage. If `R` reset the machine, one wrong key could make you set up
> the machine again.

## Editing the command line

The monitor has a full line editor. The key with the label Backspace erases the character to the
left of the cursor. It does this for each byte that a terminal can send for that key. The arrow
keys move the cursor in the line. The line editor also keeps a **command history**. The up arrow goes
back through the lines that you typed, and the down arrow goes forward to the line that you were
typing.

The history is **saved between sessions, for each directory**. When you quit, the monitor
writes your last commands to a hidden file, `.pascal100_history`, in the directory where you
started pascal100. The next time you start pascal100 in *that* directory, the up arrow
shows those commands. Each project directory keeps its own list. The monitor writes the history file
only when you type at a real terminal. A script, a pipe or an automated run never makes a
history file.

The `history` setting on the console sets how many lines the history file keeps. The default is 50. Type
`SET CONSOLE history=200` to keep more lines. Type `SET CONSOLE history=0` to stop saving the
history file.

### Completing with `Tab`

`Tab` completes the word that you are typing. It gets the possible words from the current
machine. When you add a board, `Tab` can complete the board's id at once. There is no list to keep up to date. `Tab` completes these words:

- at the start of a line, a **command**: `SH`⇥ → `SHOW`
- after `SET`, a **board id**, or `CONSOLE` or `DISPLAY`: `SET me`⇥ → `SET mem0`
- after a board id, one of **the board's property names**, with the `=` added:
  `SET mem0 fi`⇥ → `SET mem0 fill=`
- after the `=`, one of **the property's legal values**: for `SET mem0 fill=`⇥ the choices
  are `zero` and `random`

When more than one word fits, `Tab` adds the letters that all of the words share, and then
stops. Press `Tab` again to see the list. When no word fits, `Tab` does nothing.

### Editing keys

| Key | Does |
|---|---|
| `←` `→` | move one character |
| `Ctrl-A` / `Home` | move to the start of the line |
| `Ctrl-E` / `End` | move to the end of the line |
| `Alt-B` / `Ctrl-←` | move back one word |
| `Alt-F` / `Ctrl-→` | move forward one word |
| `Backspace` | erase the character before the cursor |
| `Delete` | erase the character under the cursor |
| `Ctrl-W` | erase the word before the cursor |
| `Ctrl-K` | erase from the cursor to the end of the line |
| `Ctrl-U` | erase the full line |
| `↑` `↓` | move back and forward through the command history |
| `Tab` | complete the word at the cursor |
| `Ctrl-D` | on an empty line, quit, the same as `QUIT` |

> **In the monitor, `Ctrl-E` moves to the end of the line.** In the monitor you type to the
> *line editor*. When a **running** guest has the console, `Ctrl-E` is **STOP**. `Ctrl-E` stops the
> machine and starts the monitor, so you have the keyboard again (see below). The same key does two jobs in two places.

## Repeating the last command: `.`

Type `.` alone on a line to run your **last command again**. The monitor does not echo the
command, so you see only its output. The commands that you usually repeat continue from where
they stopped. A bare `DISASM` disassembles the next screen, a bare `DUMP` shows the next page,
and `STEP` steps again. To go forward through a routine, type `DI` one time and then `.` `.` `.`.
To single-step, type `S` and then `.`.

Each `.` repeats the original command, not the previous `.`. Ten presses of `.` run the original command ten times. If you type `.` before any other command, the monitor tells you
that there is nothing to repeat.

## Reaching the host: `!`

A line that starts with `!` goes to **your host shell**. The monitor sends all the text after
the `!` to the shell with no changes, spaces included. The monitor waits until the command ends,
and then it shows the `pascal100>` prompt again:

```
!ls                 list the directory you started from
!vi HELLO.PRN       open a file in your editor, then :q to return to the monitor
!cp game.dsk save.dsk   copy a disk and keep it mounted
```

`!` runs **your** shell, with your own permissions. The shell is not part of the machine,
and the guest cannot see it or use it. An editor works because the monitor releases the keyboard
before it starts the command. `vi` gets a normal terminal, and it gives the terminal back when it exits. The machine stays stopped while the command runs, as it always is while the monitor runs.

A bare `!` with no command shows how to use `!`.

Under `--mcp`, the monitor refuses `!` and runs nothing. Standard output carries the messages
between the server and its client, and the output of a shell command would break them.

## Numbers: one fixed rule

> **On the wire → hex. Never on the wire → decimal.**

If the processor can see a number, the number is **hex**. Addresses, ports, data bytes and registers
are hex. If the processor never sees a number, the number is **decimal**. Counts, widths, sizes and
drive numbers are decimal.

**Hex is only the *default* for the wire class.** If you set the console to octal, the monitor
reads and prints the wire class in octal. Some period manuals, such as those of MITS, used octal.
The rule stays the same. Octal replaces hex for the wire class, and counts stay decimal. *Reading
and writing in octal*, below, tells you how to use octal.

```
DUMP 100            address  -> 0100 hex
STEP 10             a count  -> ten instructions
OUT FF 55           port and byte -> both hex
SET if1:a baud=9600       a baud rate -> nine thousand six hundred
```

A prefix or a suffix sets the base of one number:

| Write | Base |
|---|---|
| `0x`, `$`, or a trailing `h` | hex |
| `0o`, or a trailing `q` | octal |
| `%` | binary, one digit for each bit |
| `0b` | binary, but only where the number is decimal or octal. Where the number is hex, `B` is a digit: `0B10` is the address 0B10. |
| a leading `#` | decimal |
| a `K` or `M` suffix | **always** decimal. `48K` is 49,152. |

The monitor rejects a number with two bases, such as `0x10K`. It does not guess.

The same rule applies in the monitor, in a machine file and in the settings of every board. You
learn only one rule.

The **classes** are fixed. Each kind of number is always in the same class. You can change the
base in which the monitor *prints* the wire class, and the next section tells you how to change it.

### Reading and writing in octal

You can use **octal** in place of hex:

```
SET CONSOLE base=octal
```

After this command, the wire class is **octal**. The monitor reads and prints addresses, ports, data
bytes and registers in **split octal**. Each byte is a group from `000` to `377`. A 16-bit
processor address is two groups, and a 24-bit bus address is three groups:

```
DEPOSIT 2000 303 064 022   writes a jump to 0x1234 at address 0x0400
EXAMINE 2000        -> 004 000  303   (the byte 0xC3 at address 0x0400)
DUMP 2000-2002      -> 000 004 000  303 064 022
DISASM 2000 1       -> 004 000  303 064 022  JP 022 064   (a jump to 0x1234)
```

A bare number is octal now too, so `100` is address `0x40`. The decimal class, such as counts,
widths and baud rates, does not change. The prefixes and suffixes still work in both modes, so
you can always type a number in the base that you want. `0x1234` is hex in octal mode, and
`0o377` is octal in hex mode. `base=hex`, the default, sets hex again. To start in octal every
time, set `[console] base = octal` in a machine file.

### Addresses: 24 bits on the bus

The bus of every pascal100 machine is IEEE 696, with 24 address lines. The monitor uses two kinds
of address:

- **A physical address** has 24 bits, from `000000` to `FFFFFF`. The monitor prints six hex
  digits, also below 64K, or three groups in octal. The commands that need no processor use
  physical addresses: `DUMP`, `EDIT`, `FILL`, `SEARCH`, `COMPARE`, `MOVE`, `LOAD`, `SAVE`,
  `WHO`, `SHOW BUS MAP`, `SHOW BUS CONTENTION`, `BREAK MEM`, `TRACE` and `HISTORY BUS`.
- **A processor address** is the address that the processor uses. The Z80 and the 8085 have
  16 address lines, so their addresses go from `0000` to `FFFF`. `EXAMINE`, `DEPOSIT`,
  `DISASM`, `REGS`, `STEP` and a PC breakpoint use processor addresses. `EXAMINE` and
  `DEPOSIT` act through the program counter, as the front panel does. On the 8088, `DISASM`
  takes a `SEG:OFF` address, as DDT86 lists it. `HELP DISASM` shows how.

```
DUMP 10100-1010F    -> 010100  A6 79 34 A3 ...   (segment 01, on a machine with 128K)
WHO 20100           -> 020100 read  nobody -- floats to FF
BREAK MEM W 10100   -> breakpoint 1: mem w  010100
```

`SHOW BUS MAP` gives one row for each segment where a board answers. A board that does not
decode A16-A23 gets one row, marked `all segments`. `TRACE` and `HISTORY BUS` show the full
16-bit I/O address, with the byte that the processor drove on A8-A15. A 16-bit transfer shows as
`MR16`, `MW16`, `IN16` or `OUT16`, with the even byte first. A memory or I/O breakpoint stops a
16-bit transfer when it names the even address or the odd address. `SAVE` writes an Intel HEX type 04 record for each
64K segment above segment 00, and `LOAD` reads it. A `.PRN` listing uses processor addresses, so
it stops at `FFFF`.

`IN`, `OUT` and `WHO IO` take an 8-bit port. Every board decodes its ports from A0-A7.

## Naming a board: `<id>[:<unit>]`

Every board in the machine has an **id**. The id is a name that you give the board when you add
it, in a machine file or with `BOARDS ADD`. An id can be any name, such as `if1` or `serial`.
Upper case and lower case are the same.

The machines that come with pascal100 use a short type name and a number, such as `cpu0`,
`if1` and `fd0`. The number has no special meaning. It only tells two boards of the same kind
apart, such as `sio1` and `sio2`.

Some boards also have **units**. A unit is a part of a board that has its own name. For example,
an Interfacer 1 serial board has two channels, `a` and `b`. A Disk 1 floppy controller has four
drives, `drive0` to `drive3`, and one serial unit, `serial`. To name a unit, type the board id,
a colon and the unit name:

```
SHOW if1                   the board
SET  if1:a baud=1200       channel a of the board
MOUNT fd0:drive1 my.dsk    drive1 of the board
```

### Shorter names

When you type in the monitor, you can shorten the id, and you can omit the unit.

**You can omit the digits at the end of an id.** `fd` finds `fd0`, and `if` finds `if1`. A shorter id
works only when one board has that id with digits after it. If the machine has `sio1` and `sio2`,
`sio` is ambiguous. The monitor then lists `sio1` and `sio2` and stops. You can omit only the
digits at the end, so `f` does not find `fd0`.

**You can omit the unit when the board has only one unit that the command can use.** For `MOUNT`,
the monitor counts only the units that take a medium, such as a drive. For `CONNECT`,
it counts only the serial units. For example, `CONNECT fd0 null` connects the one serial unit of
`fd0`. A Disk 1 has four drives, so `MOUNT fd0 my.dsk` does not work. The monitor
lists the four drives and stops.

To change a property of a unit, `SET` always needs the unit name. Without a colon, `SET` changes
a property of the board.

Shorter names work only in monitor commands. A machine file, and an MCP tool that takes a board
id, must use the full id and the full unit name.

## Seeing the machine

This is the built-in `default` machine, a CompuPro workbench:

```
pascal100> BOARDS
  ID    TYPE         I/O          UNITS                                                                                 MEMORY
  ----  -----------  -----------  ------------------------------------------------------------------------------------  -------------------------------------------------------------------------------------------------------------------------------
  cpu0  z80          -            1 cpu: z80                                                                            -
  if1   interfacer1  00,01,02,03  2 serial: a*, b                                                                       -
  fd0   disk1        C0,C1,C2,C3  4 disk: drive0(empty), drive1(empty), drive2(empty), drive3(empty); 1 serial: serial  0000-FFFF  boot ROM  64K  every memory read gets byte A0-A7 of one 256-byte routine, over PHANTOM* until OUT BASE+3 with D0 = 0
  hb0   hostbridge   B0,B1        -                                                                                     -
  mem0  memory       -            -                                                                                     000000-00FFFF  ram  64K  ext  16-bit

  * holds the console
```

The `BOARDS` output is the backplane. It shows each board in the machine, the ports and memory that the board
decodes, and what is in its units.

| Command | Shows |
|---|---|
| `BOARDS` | the backplane |
| `SHOW <id>` | one board: each setting, its value, and the values that it accepts |
| `SHOW MACHINE` | the whole machine |
| `SHOW MOUNTS` | every disk and ROM in the machine, and what is in it |
| `SHOW CONSOLE` | which unit has your keyboard, and which transforms apply to its bytes |
| `SHOW DISPLAY` | the windows pascal100 opens: whether a window comes to the front when it opens, where its keys go, and whether the CRT look is on |
| `SHOW SERIAL` | the serial ports on your computer, and the unit that holds each port |
| `SHOW CLOCK` | the emulated time: T-states since `POWER`, and what they are in seconds |
| `SHOW BUS` | the whole backplane at one time: `MEMORY`, `I/O`, a short `INTERRUPTS` summary, and `DMA` when there is a DMA board |
| `SHOW BUS MAP` | which board decodes each address, and which addresses float |
| `SHOW BUS IO` | which board decodes each port |
| `SHOW BUS IRQ` | which board is strapped to each interrupt line, and which board asserts it |
| `SHOW BUS DMA` | each DMA board and its TMA priority, highest first, and who pulls pHOLD |
| `SHOW BUS CONTENTION` | where two boards decode the same address or port |

`HELP SHOW` lists all the forms of `SHOW`.

Look closely at `SHOW <id>`, because it is the **only** command that you need to configure a
board. It lists every property, its value and its legal values. The property names **are** the
keys that you write in a machine file. pascal100 has no second schema. The board reference
in the *User Manual* is printed from the same tables. (The reference at the end of this document
is for the monitor's commands.)

## Changing the machine

```
SET cpu0 clock_hz=4000000      run at a real 4 MHz clock
SET mem0 fill=zero             RAM starts as zeros, not random bytes
SET if1:a baud=19200           set channel a of the Interfacer 1 to 19200 baud
BOARDS ADD 2sio sio1 port=10   add a 2SIO serial board at port 10
BOARDS REMOVE sio1             remove it
CONFIG SAVE mine.toml          save the machine as it is now
```

`pascal100 mine.toml` starts the same machine that `CONFIG SAVE` saved.

## Running, and stopping

```
RUN 0        set the PC and start: the same two steps as on the front panel's switches
RUN          continue from where the processor stopped
```

On the CompuPro machines, `RUN 0` boots the disk: after a reset, the Disk 1 boot EPROM answers at
`0000`.

**`RUN <addr>` is EXAMINE followed by RUN**, as on the front panel. pascal100 has no `BOOT`
command, and this is deliberate. A machine starts only when a `RUN` tells it to start. A machine
file can give the `RUN` for you: the example machines have `startup = ["RUN 0"]`.

If a board holds the console, **the guest gets the keyboard**. The guest gets every key,
including `Ctrl-C`, because a CP/M program can read `Ctrl-C`.

### `Ctrl-E` stops the machine and starts the monitor

**`Ctrl-E`** is the front-panel STOP switch. pascal100 reads `Ctrl-E` *before the guest can see
the byte*. The guest never gets `Ctrl-E`, and it cannot disable `Ctrl-E`. `Ctrl-E` always stops the
machine.

If the guest needs `Ctrl-E`, move STOP to a different control key. For example, type
`SET CONSOLE stop=1D` to use `Ctrl-]`. The value is the key's control code, from `01`
(`Ctrl-A`) to `1F` (`Ctrl-_`). To use the new key every time, set `[console] stop = 0x1D` in a
machine file.

**`Ctrl-E` stops the machine and starts the monitor.** The guest no longer has the keyboard, and you
type to the monitor. No instruction executes while the monitor runs. This is CP/M 2.2 on the
`examples/cpm22` machine, stopped at the `A>` prompt:

```
A>
STOP -- the machine is still at F4BD. RUN resumes.
C0Z1S0P1H1N0 A=00 BC=007F DE=DD06 HL=E80E SP=E837 IFF1=0 PC=F4BD  IN A,(01)
C0Z0S0P0H0N0 A'00 BC'0000 DE'0000 HL'0000 IX=0000 IY=0000 I=00 IM=00 IFF2=0
pascal100>
```

STOP does not *change* the machine, because STOP is not RESET and not POWER.
The registers, the memory and the disks stay as the guest left them. A bare `RUN`, with no
address, continues at the instruction that the processor was about to execute. The message
*"still at F4BD"* tells you this.

### How a run ends

A `RUN` ends when it reaches a **breakpoint**, or a `HLT` that no interrupt can end. It always
tells you which. If no board holds the console, the guest does not get the keyboard. To stop
that run, press `Ctrl-C`.

After a `HLT`, the processor stays halted until `RESET`. On a halted processor, `RUN`,
`EXAMINE` and `DEPOSIT` do nothing, as the front-panel switches do. The monitor tells you:
*"processor halted at PC=0402 -- RESET ends the halt."* *The Debugger* has the details.

**`RUN` and `Ctrl-E` are the front panel's RUN and STOP switches.** `RUN` starts the processor. STOP, a
breakpoint or a `HLT` stops the processor and starts the monitor. **The monitor is
available only while the machine is stopped.** While the machine runs, the guest has the keyboard
and there is no `pascal100>` prompt. Every `SET`, `DEPOSIT` and `EXAMINE` acts on a *stopped* machine. A property never changes during an instruction. No property is locked while the machine
runs, and no property can be set only while it runs.

## Speed

**By default, the machine runs as fast as the host can run it.** `clock_hz` on the CPU board is
`0`. For example, a disk copy that took a real machine a minute ends in a moment.

Type `SET cpu0 clock_hz=4000000` to run at the speed of a real 4 MHz processor. The guest sees
the same result at either speed, because each instruction uses the same number of T-states. The
real clock speed changes how the machine *feels*, not how it *behaves*. `SHOW cpu0` shows
`achieved_hz` next to `clock_hz`. `achieved_hz` is the clock speed that the run loop reached.
You can read it, but you cannot set it.

A crystal is always kept: at the prompt, from a script, from a pipe and under `--mcp`. A script
that must be fast leaves `clock_hz` at 0, or starts with `SET cpu0 clock_hz=0`. `SHOW CLOCK`
tells you which: its `pacing` line reads `paced` or `free`.

The speed matters to a guest that measures time against the *outside* world. An XMODEM transfer
needs the real clock speed, and a disk copy does not. For more information, see `clock_hz` and
`idle` in the Boards chapter of the *User Manual*, and see its Troubleshooting chapter.

## RESET is not POWER

| | |
|---|---|
| `RESET` | the bus's RESET* line. The processor restarts at its reset address. **Memory stays the same**, and the disks stay mounted. |
| `POWER` | a power cycle. **This is the only command that loses the contents of RAM.** It also reads the ROM images again. |

`RESET` does not clear memory, because RESET on a real S-100 machine did not clear memory. Much
period software needs this behavior.

`RESET*` is a **line on the backplane**. It is not an instruction that pascal100 does for
you. Every board receives it, and each board does what its real hardware did. The result is
different for each board. A `memory` board does not change RAM, because a RAM chip has no reset
pin. The Disk 1 turns its boot EPROM on again, so the
processor finds the boot routine at `0000`.

A 2SIO does **nothing**, because the 6850 has no reset pin for `RESET*`. Its baud rate, word
format and interrupt enables stay the same after a reset, as on real hardware. If you type
`RESET` after a guest fails, a 2SIO port stays as the failed guest set it. All of RAM stays the
same.

**`POWER` uses a different line.** When the machine powers on, it drives `POC*` (Power-On
Clear), which is a separate backplane line. A board can respond to `POC*` and `RESET*` in
different ways, because the real cards did. The System Support 1 is an example. Its 8253 timer
and its two 8259A interrupt controllers have no reset pin, so `RESET*` does not reach them. If a
crashed guest left the interrupt controllers armed, they **stay armed after a `RESET`**. Only
`POWER` clears them.

`POC*` is also the only time when RAM loses its contents. On `POWER`, the memory board fills RAM
again, with **random bytes by default**, because static RAM does not start with zeros. It also
reads every ROM image again.

| | The processor | The boards | RAM |
|---|---|---|---|
| `RESET` | restarts at its reset address | `RESET*` on the bus. Each board does what its hardware did, and some do nothing. | **stays the same** |
| `POWER` | restarts at its reset address | `POC*` on the bus. Each board starts as it did at power-on. | **filled again**, ROMs read again |
