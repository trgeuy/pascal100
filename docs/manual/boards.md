# Boards

An S-100 machine is a **backplane**, and everything in it is a board. The memory, the serial
ports, the disk controller and the processor are all boards. Without the boards, only the bus is
left.

`pascal100` works in the same way. For this reason, the crystal is a property of the processor
*board*, not of the machine. You can remove a board, add a different one, and see what the
software does.

This chapter starts with the commands that add, remove and show boards. After that, it tells you
what each board **is**: what the real hardware was, what it is for, and what can go wrong. **It
does not list the properties of each board.** The board reference at the back of this manual
lists every property of every board, from the program's own tables.

## Add, remove and look at boards

You can do everything by hand that a machine file does to a board.

| Command | |
|---|---|
| `BOARDS` | what is in the backplane |
| `SHOW BOARDS` | the board types that you can add, one line each |
| `SHOW BOARD <type>` | the description and settings of one type (add `UNITS` for only the units) |
| `BOARDS ADD <type> <id>` | add a board |
| `BOARDS REMOVE <id>` | remove a board |
| `SHOW <id>` | the settings of one board in the machine, with the values that it accepts |
| `SET <id> <key>=<value>` | change one setting |

```
pascal100> BOARDS ADD io4 io40
pascal100> SET io40 port=10
pascal100> SHOW io40
```

**The keys at the prompt are the keys in a machine file.** `SET cpu0 clock_hz=2000000` at the
prompt and `clock_hz = 2000000` in a machine file set the same property.

When you have the machine that you want, type:

```
pascal100> CONFIG SAVE mine.toml
```

This writes the machine to a file, and the file loads back as the same machine.

### Look at a board type before you add it

`SHOW BOARD <type>` reads the *catalog*. It makes one board of that type, describes it, lists
its settings, and removes it again. Nothing is added to the backplane. For a disk controller,
the last lines name its units:

```
pascal100> SHOW BOARD disk1
  disk1  CompuPro Disk 1 (1981), the Disk 1A's predecessor: an NEC uPD765A
         floppy controller for 8" drives, single and double density, up to 4
  ...
  This board has units: serial, drive
  SHOW BOARD disk1 UNITS for their properties.
```

Under the legal values of each setting, `SHOW BOARD` gives a `default:` line. It is the value
that the setting has when a machine file does not set it. A setting that only shows live state
has no default, and has no line:

```
pascal100> SHOW BOARD 2sio
  PROPERTY  HELP
  --------  ------------------------------------------------------------------
  port      Base address. The board decodes four ports: BASE+0 .. BASE+3
            values: 0x0 .. 0xFC
            default: 0x10
```

Add `UNITS` to see only the units. Each unit has a heading with its **kind**, and after the
kind, the **command that fills it**:

```
pascal100> SHOW BOARD disk1 units

  Unit 'serial'  (serial, CONNECT)

  PROPERTY  HELP
  --------  ------------------------------------------------------------------
  baud      Line rate: the far end's setting, and what the guest's delay loop
            must match. 8 data bits, no parity, 1 stop bit
  connect   The endpoint on the other end of the line (CONNECT sets this)
  ...

  [[board.drive]]  (in a machine file)

  unit      Which drive (0..3)
  mount     The disk image to put in it. Relative to THIS FILE.
  ...
```

- A **serial** unit is an endpoint, so you `CONNECT` it, to `console`, a socket or a real port.
- A **disk** or **rom** unit holds an image, so you `MOUNT` a file in it.
- A **cpu** unit is fixed on the board. It takes neither command, and the heading shows only its
  kind.

The heading tells you which command a unit takes, before you add the board.

When a *machine file* fills a unit, `UNITS` shows the TOML table and its keys instead. For a
disk controller, this is `[[board.drive]]` with its `mount`, `readonly` and `media` keys. You
can find the file form as easily as the prompt form.

## Contention

When two boards decode the same port, that is **contention**, and it happened on real backplanes
too. Both boards answer and both drive the data bus, so the processor reads the wrong value.

The simulator does not stop you from doing this. **It tells you about it.** For example, add a
second Interfacer 1 to the `default` machine, which already has one at the same ports:

```
pascal100> BOARDS ADD interfacer1 if2
if2: interfacer1 added
pascal100> SHOW BUS CONTENTION
  port 00 OUT driven by if1 if2
  port 01 OUT driven by if1 if2
  port 02 OUT driven by if1 if2
  port 03 OUT driven by if1 if2
```

The output names each port and both boards. You can build a machine that does not work, on
purpose, because a wrong setup is part of developing hardware. The program shows you what is
wrong.

A few boards are made to share ports. Up to eight CompuPro **M-Drive/H** boards sit at the same
two ports: each board takes every write, and only one board drives a read. The simulator knows
this, so two M-Drive/H boards at the same ports are not contention. Another type of board at
those ports still is.

## The boards

The boards are in groups, in the same order as the sections below.

**Memory**

| Type | What it is |
|---|---|
| `memory` | RAM and ROM, as a list of regions |

**Processors**

| Type | What it is |
|---|---|
| `8085` | an 8085 processor board. It runs all 8080 code, and adds RIM, SIM and the TRAP and RST interrupts |
| `z80` | a Z80 processor board. The same bus, with a different instruction set |
| `pascal100` | Digicomp Research Pascal-100: a Z80 and the Western Digital MicroEngine, one active at a time |
| `cpu8687` | CompuPro CPU 86/87: an 8086 on a 16-bit bus. Boots CompuPro CP/M-86 and MP/M 8-16 from their Disk #2 |
| `spuz` | CompuPro SPUZ: a slave processor with its own Z80, RAM and two serial ports. Up to 16 boards. Not a bus master |

**Serial ports and consoles**

| Type | What it is |
|---|---|
| `2sio` | MITS 88-2SIO: two serial ports |
| `io4` | SSM IO-4: two serial ports and four parallel ports |
| `interfacer1` | CompuPro Interfacer 1: two serial ports with a control port and interrupts. The console of the CompuPro machines |

**Floppy and disk controllers**

| Type | What it is |
|---|---|
| `tarbell` | Tarbell #1011: a single-density floppy controller with its own boot PROM. Boots CP/M by itself |
| `tarbelldd` | Tarbell #2022: the double-density version, for mixed-density disks |
| `disk1` | CompuPro Disk 1: an 8″ floppy controller with DMA, its own boot EPROM and a bit-banged serial line. Boots CompuPro CP/M 2.2 and UCSD Pascal on the Pascal-100 |
| `disk1a` | CompuPro Disk 1A: an 8″ floppy controller with DMA and its own boot EPROM. Boots CompuPro CP/M |
| `jadedd` | Jade Double D: a double-density floppy controller with its own Z80. The host loads the program that the Z80 runs |
| `mdriveh` | CompuPro M-Drive/H: 512K of RAM used as a disk drive, on two ports. Up to eight boards. CompuPro's systems use it as drive M: |

**Interrupts and the clock**

| Type | What it is |
|---|---|
| `ss1` | CompuPro System Support 1: a real-time clock, a serial channel, an interval timer, two interrupt controllers and a math socket |

**Host integration**

| Type | What it is |
|---|---|
| `hostbridge` | file transfer to your computer. **Our own board, not a period board** |

## Memory

RAM, ROM, and the boards that switch banks of RAM.

### `memory`: RAM and ROM

A memory board is **a list of regions**. A real S-100 memory board was the same. It had banks of
chips, and each bank decoded the addresses that its jumpers set. A board with RAM at the bottom
and a monitor PROM at the top was an ordinary board.

`default` has one memory board with 64K of RAM.

#### `PHANTOM*`: how a boot PROM gets out of the way

The bus has a line called `PHANTOM*`. **A board pulls this line to switch another board off.**
After a reset, the boot EPROM of the Disk 1 answers memory reads at `0000`, and it asserts
`PHANTOM*`. The RAM board under it stops answering reads, if its jumper tells it to obey
`PHANTOM*`. Two boards decode `0000`, and only one answers.

This is how a CompuPro machine boots from its disk controller. The EPROM covers the RAM at the
bottom of memory, and the loader runs from the EPROM. Writes still go to the RAM under it
(`honors_phantom = "read"`), so the loader can put the system in the memory that it covers.

Whether a board obeys `PHANTOM*`, and whether it asserts it, are **jumpers** on the board. You
set them. If you set them wrong, the machine does not boot, and it does not tell you why. The
bus view in the monitor shows you both boards on the same page.

A `memory` board has no bank switching.

## The processor boards

The processor boards are `8085`, `z80`, `pascal100` and `cpu8687`. This section applies to all
of them. The sections after it describe each one.

**The processor is a board like the other boards.** It plugs into the backplane, and you can
remove it. With `-n`, you can build a machine with no processor. The `8085` and `z80` boards
decode no ports and no addresses. They run the program, and they drive the bus to do that. You
can put a `z80` where an `8085` was, and the bus, the other boards and the debugger all work the
same. The `pascal100` board also has two ports of its own.

Every processor board has the same three properties:

- **`clock_hz`**, the crystal
- **`idle`**, which lets the processor rest at a prompt
- **`achieved_hz`**, a read-only value: the speed that the processor reached

### The crystal is on the board: `clock_hz`

The crystal is on the processor board, so `clock_hz` is a property of the board, not of the
machine. **`clock_hz = 0` is the default, and it means "run as fast as possible".** On a modern
computer, that is many times as fast as a real processor board.

To run at 2 MHz, type:

```
SET cpu0 clock_hz=2000000
```

To use that speed for one run of a machine file, and leave the file as it is, give the setting
on the command line: `pascal100 mine.toml --set "cpu0 clock_hz=2000000"`. The machines chapter
describes `--set`.

**The guest sees the same result at either speed.** Each instruction takes the same number of
T-states. The crystal changes how the machine feels to you, not what it does.

There is one exception, at the edge of the machine. A guest counts instructions to measure time.
At full speed, a guest's "three-second" timeout passes in a few milliseconds of your time. For
this reason, a guest that times something outside the machine, such as XMODEM through a serial
port, needs the real crystal. The troubleshooting chapter tells you more.

To see how much time has passed for the guest, type `SHOW CLOCK`:

```
pascal100> SHOW CLOCK
clock  (emulated time -- T-states since POWER, and what they are worth)

  elapsed    0.102953 s   (205905 T-states)
  crystal    2000000 Hz   SET cpu0 clock_hz=N
  pacing     paced -- emulated seconds keep step with real ones

  Elapsed is the GUEST's time, counted from the crystal above: the same
  division a 9600-baud line does to turn a character into T-states. It is
  not how long you have been sitting here, and running flat out is exactly
  when the two differ most.
```

The program counts the T-states since power on, and divides them by the crystal. The result is
the guest's time, not your time. A CP/M boot takes the same guest time at full speed and at 2
MHz. Use this number to measure a guest's own timeout.

**The `pacing` line tells you if a run keeps real time.** It reads `paced` when a crystal is
set, and `free` when `clock_hz` is 0. A crystal is always kept, also when you start the program
from a script with no terminal. If a script must be fast, leave `clock_hz` at 0, or put
`SET cpu0 clock_hz=0` at the start of the script.

### `idle`: the processor rests at a prompt

At a prompt, a guest only reads the serial status register again and again, and waits for a key.
At full speed, that keeps one core of your computer busy. **`idle` lets the processor rest while
the guest reads an empty keyboard.** It is on by default. The busy core then uses a few percent.
**The guest cannot see the difference**, because the processor starts again as soon as a byte
arrives, before the next read. An XMODEM transfer is correct with `idle` on.

`idle` has an effect only at full speed. When you set a crystal with `clock_hz`, the program
already rests to keep the crystal's time, and the guest's time at a prompt stays correct.

### `8085`: an 8085 processor

**The 8085 came after the 8080, and it runs every 8080 program** without a change. Of the processor
boards, it is the nearest to the 8080. Everything in *The processor boards* above applies.
This section gives only what the 8085 adds.

The 8085 adds `RIM` and `SIM`, which read and set the interrupt mask and the SID and SOD serial
pins. It also adds its own interrupts, `TRAP`, `RST 5.5`, `RST 6.5` and `RST 7.5`, as well as
the 8080's `INTR` line. The documented instructions are correct. This includes the one
instruction that is different from the 8080: `ANA` and `ANI` always set the auxiliary carry. The
undocumented instructions run too, and set the extra V and K flag bits. `DISASM` marks each
undocumented byte, as `DDT` does.

No board drives the 8085's own *pins*: the `SID` and `SOD` serial lines, and the `TRAP`,
`RST 5.5`, `RST 6.5` and `RST 7.5` interrupt inputs. The S-100 bus does not carry them. Ordinary
interrupts on the `INTR` line work as they do on the 8080.

### `z80`: a Z80 processor

**The processor of the CompuPro machines.** It uses the same backplane, with a different
instruction set. Everything in *The processor boards* above applies. The built-in `default`
machine and the CP/M examples have a `z80`.

### `pascal100`: Digicomp Research Pascal-100

The **Pascal-100** (Digicomp Research, 1981) is a two-card processor set. It has a **Z80** and
the **Western Digital Pascal MicroEngine**, which runs UCSD Pascal III.0 P-code in its microcode.
**Only one processor runs at a time.** The other is held.

After a reset, the Z80 has control, so the board starts as a plain Z80 board. The Z80 boots the
disk and then starts the MicroEngine. While UCSD Pascal runs, the MicroEngine asks the Z80 to do
the disk and console I/O.

The board has two ports, both write-only: the **System Control Port** (default `D0`) and the
**Map Output Port** at the next address. The map gives each processor 128K of memory, in 16
pages of 8K. The board works on the IEEE 696 bus only.

The Pascal-100 machine (`examples/pascal/pascal.toml`) has this board, an Interfacer 1 console,
a Disk 1 and 128K of RAM. It boots a UCSD Pascal III.0 system disk that was made for this
hardware. The example has one, `pascal-dd.dsk`, and boots it at once:

```
$ cd examples/pascal
$ pascal100 pascal.toml
```

For a disk of your own, change the `mount` line in the file, or mount it and start the boot
EPROM:

```
pascal100> MOUNT fd0:drive0 PASCAL.DSK
pascal100> RUN 0
```

### `cpu8687`: CompuPro CPU 86/87

The **CompuPro CPU 86/87** (1982) is an **8086** processor board. The 8086 moves 16 bits at a
time when the memory board can, and one byte at a time when it cannot. A `memory` board with
`sixteen_bit = true` takes the 16-bit cycles. The board runs **no 8-bit software**: for 8080
programs, CompuPro sold the CPU 8085/88.

The 8086 starts at `FFFF0`. The Disk 1 boot EPROM answers there while it boots, so set the
Disk 1 to an 8086 routine: `routine = 0` for the Interfacer 1 console at `00`/`01`, or
`routine = 2` for the System Support 1 serial channel. On the real board that is jumper J17 at
B. CompuPro's CP/M-86 and MP/M 8-16 boot from their **Disk #2** on this board; Disk #1 does
not. To make an MP/M 8-16 disk boot, put the CPU 86/87 loader on it from CP/M-86:

```
A>B:MPMGEN B:MPMLDRFY.86
```

**To boot, run from `FFFF0`, not from 0:** `RUN FFFF0` at the monitor, or
`startup = ["RUN FFFF0"]` in the machine file. Over MCP it is `run {from: 1048560, until: "A>"}`,
because `from` takes a decimal number. `RUN 0` starts the 8086 in the boot EPROM's 8080 code, and
it does not boot. A bare `RUN` also works after power-on, because the 8086 is still at its reset
address. Like `RUN 0` on an 8080 machine, `RUN FFFF0` boots again only while the boot EPROM is
on: after the guest turns it off, `RESET` first.

A machine file:

```toml
[machine]
startup = ["RUN FFFF0"]

[[board]]
type = "cpu8687"
id   = "cpu"

[[board]]
type    = "disk1"
id      = "fd0"
routine = 0
```

The board has two functions of its own:

- **The Memory Manager**, port `00FD`. The top four bits of an `OUT` to it drive address lines
  A20-A23 from the next cycle on. RESET clears it.
- **The 80130**, ports `FFF0`-`FFFF`: an interrupt controller and three timers. CompuPro's
  systems use the interrupt controllers of the System Support 1; the 80130 is there for
  programs that want its timers.

| Property | Default | What it does |
|---|---|---|
| `clock_hz` | `0` | the 8086's clock: 8000000 or 10000000. 0 runs flat out |
| `mm_port` | `00FD` | the Memory Manager's port. The low byte is always FD |
| `port_80130` | `FFF0` | the first of the 80130's 16 ports |
| `io_80130` | `true` | the 80130's ports answer |
| `io_wait` | `true` | one wait state on every I/O cycle |
| `power_on_jump` | `false` | start at `poj_addr` instead of `FFFF0` |
| `jump_on_reset` | `false` | do the power-on jump at every RESET too |
| `poj_addr` | `FF000` | where the power-on jump goes: a 4K boundary below 1 MB |

Not modeled: the 8087 math chip (the socket is empty), the 80130's ROM, and the jumpers that send
the 80130's interrupt and timer outputs to the bus. With those jumpers open, as in CompuPro's
standard system, the 80130 interrupts nothing.

### `spuz`: CompuPro SPUZ

The **SPUZ** (Viasyn, 1985) is a slave processor board for the CompuPro 816 and 286 systems. It
has an 8 MHz **Z80**, 64K or 256K of RAM, and a **2681** with two serial channels. Under
Concurrent DOS 8-16, each board runs 8-bit programs for one user while the host runs 16-bit
programs.

The SPUZ is not a bus master. Its Z80 runs on the board's own bus, and it runs only while the
host processor runs. The host sees the board as a 64K memory window and ports `FC30`-`FC41`.
The board decodes all 16 bits of the port address, so it does not answer port `0030`.

| Port | What it does |
|---|---|
| `FC30`-`FC3F` | The registers of the selected board's 2681. |
| `FC40` | SLAVESEL (write only). Every board compares bits D3-D0 with its `select`. The board that matches is selected. |
| `FC41` | PTLATCH (write only), on the selected board. D3: `1` lets the Z80 run, `0` resets the Z80 and the 2681. D1-D0: the 64K bank that the host sees. |

Up to 16 boards share the window and the ports. Only the selected board answers the window,
the 2681 ports and PTLATCH. Give each board its own `select`. A machine with two boards on one
select does not load or run.

| Setting | Values | What it does |
|---|---|---|
| `select` | `0`-`15` | The board select. The default is `0`. |
| `window` | `00`-`FF` | Address bits A23-A16 of the window. The default is `0E`, the CompuPro standard (`0E0000`-`0EFFFF`). |
| `memory` | `256k`, `64k` | The RAM. A 256K board has four 64K banks. The default is `256k`. |
| `waits` | `1`-`3` | The wait states on each host access to the window or the 2681. The default is `2`. |
| `m1_wait` | `true`, `false` | One wait state in each opcode fetch of the board's Z80. The default is `true`. |
| `interrupt` | `none`, `vi0`-`vi7` | The vectored interrupt line that the board pulls. The default is `vi0`. |
| `history` | `true`, `false` | Record the board Z80's instructions for `HISTORY CPU`. The default is `false`. |

The units `a` and `b` are the two serial channels. Connect them as any serial unit, for example
`CONNECT spuz0:a socket:2325`.

After power-on or a bus reset, no board is selected, and every Z80 is in reset. The 2681 output
OP6 is high after a reset, so **the board pulls its interrupt line** until software sets bit 6
of the 2681's output port.

**Do not read or write the window or ports `FC30`-`FC3F` while the board's Z80 is in reset.** On
the real board, this access stops the S-100 bus forever. `pascal100` lets the access go nowhere
(a read gives `FF`), prints the address and the PC, and stops the run with **bus-hang**. Select
the board, write `0B` to `FC41`, and then access the board. To load a program, halt the Z80
first: put a `HLT` (`76`) at address 0, reset the Z80, and let it run again.

`0B` sets D3 (the Z80 runs) and gives the host bank 3 (D1-D0), the bank the Z80 runs from after
a reset. With `08`, the host writes the `HLT` into bank 0 and the Z80 does not see it. A 64K
board has one bank, so `08` also works there.

**Leave a hole in the RAM for the window.** CompuPro's `SPZ.CMD` finds each board by writing
`E0000` and reading it back. RAM under the window answers for a select that has no board, so every
select looks like a board. With the window at `0E` and 1 MB of RAM:

```toml
[[board]]
type = "memory"
id   = "mem0"
ext  = true
honors_phantom = "read"   # for the Disk 1A boot ROM, as in the other Concurrent DOS machines
  [[board.region]]
  type = "ram"
  at   = 000000
  size = "896K"
  [[board.region]]
  type = "ram"
  at   = 0F0000
  size = "64K"
```

Concurrent DOS 8-16 then reports 896K. To run 8-bit programs on the board, install CompuPro's
switch and start the SPUZ manager:

1. Copy `SW!SPUZ.CMD` to `A:SW!.CMD`. Mark it with `SET A:SW?.CMD [SYS RO F1=ON F3=ON]` (CompuPro's
   `SPUZ.BAT`; `!` separates commands on the command line, so `SW?` names the file).
2. Put `SPZ` in `A:AUTO.BAT`, or type `SPZ` once after each boot.
3. Mark each 8-bit program: `SET NAME.COM [F1=ON F2=ON F3=ON F4=ON SYS RO]` (CompuPro's `8BIT.SUB`).
   Then type its name.

Each board runs one task in each 64K bank: four on a 256K board. Concurrent DOS gives the next task
to the board with the most free banks.

`SHOW` gives the PC of the board's Z80, its state, the two banks and the interrupt line:

```
  board Z80  PC 0001  halted  Z80 bank 3  host bank 3  selected  VI pulled
```

**The board's Z80 is a core for the monitor: `spuz0:z80`.** `REGS spuz0:z80`,
`DISASM spuz0:z80`, `DUMP spuz0:z80 <addr>` and `STEP spuz0:z80` work on it. `DISASM` and
`DUMP` read the bank that the Z80 runs in. `STEP` runs only the board's Z80, and the host does
not run. `BREAK spuz0:z80 <addr>` stops the board's Z80 at that address. The host then stops
after its next instruction, and `RUN` lets both go on. A breakpoint on the board's Z80 takes no
`IF` and no `TRACE`.

The board records the Z80's instructions only when you ask, because the recording costs time
on every instruction:

```
pascal100> SET spuz0 history=on
pascal100> RUN
pascal100> HISTORY CPU spuz0:z80 20
```

Not modeled: DMA into SPUZ memory (VDMA), and the time that host accesses take from the
board's Z80.

## Serial ports and consoles

The boards that have a serial port, for a console, a modem or both.

### `2sio`: MITS 88-2SIO

Two **6850 ACIAs**, units `a` and `b`, with four ports at BASE+0 to BASE+3. The base is `10` hex
by default, which is where every listing of the period expects it.

This was the usual console board of MITS machines. The CompuPro machines of pascal100 use the
Interfacer 1 instead.

#### The two halves are independent

**The two units are two separate chips on the same board.** Each unit has its own baud rate, its
own connection and its own interrupt setting. For example, unit `a` can be a console at 9600
baud, and unit `b` a modem at 1200 baud. One can use interrupts while the other is polled.

The serial chapter tells you what you can connect a unit to: your terminal, a TCP socket, or a
real serial port on your computer, with the modem control lines.

### `io4`: SSM IO-4 (2P + 2S)

**`io4`** is the **SSM IO-4**. It
has **two full-duplex serial channels *and* four latched parallel ports**. Each serial channel
is a real UART with **programmable word length (5 to 8 bits), parity, stop bits and baud**. The
status byte can be strapped in many ways, as on the real board.

The **two serial channels** are units **`a`** (Serial A) and **`b`** (Serial B). Each has its
own `[board.unit.a]` or `[board.unit.b]` table, its own `baud` and its own `connect`. The serial
half uses a block of four ports, which must start at a multiple of four (default `0`–`3`). `a`
uses ports `0` and `1`, and `b` uses ports `2` and `3`.

#### Set the straps, or select a profile

The IO-4 had many jumpers, and each jumper is a property here:

- which data bit means that a byte is waiting (`stat_dav`)
- which data bit means that the transmitter is ready (`stat_tbmt`)
- four more status signals: `stat_teoc`, `stat_ror`, `stat_rpe` and `stat_rfe`
- whether the whole status byte is inverted (`invert_status`)
- whether the status port and the data port change places (`port_reversal`)

You seldom set these one at a time. A **`profile`** sets them to match a known host:
**`altair-rev1`** (the default, the MITS SIO Rev 1 console that the SSM 8080 monitor expects),
`altair-rev0`, `i8251`, `proctech`, `imsai`, and `custom` (all straps free for you to set).
Select a profile, and then change any strap, as you would move a jumper.

You can strap the three UART **error flags** (parity, framing and overrun) to the status byte,
but they always read as inactive. The serial line carries exact bytes and has no line noise. The
current-loop and RS-232 options of the real board are electrical, not programmable, so they are
not modeled.

#### The parallel half

The other half is **four 8212 latched ports**, two for input and two for output, as units
**`pa`** and **`pb`** (Parallel A and B). They use their own block of two ports (default `4` and
`5`), which must start at an even port. An input port latches a byte on its strobe and sets a
service-request flip-flop. A read gets the byte and clears the request. `CONNECT io40:pa …`
connects a port to a source or a sink of bytes, like a serial line. The `dav_bit`, `dav_source`
and `dav_active_low` straps set up a console with its status byte on one port and its data byte
on the other.

**If the two blocks of ports overlap, neither half answers** on the shared ports. The real board
was designed this way. It is not two boards on the same address.

#### Interrupts, if you strap them

The **W4 header** of the board sets the interrupts, and `io4` follows it. Each serial channel's
receive (`rx_int`) and transmit (`tx_int`), and each parallel input (`int`), can go to a
vectored interrupt line, to the plain interrupt pin, or to `none`. **There is no software
enable.** The strap is the enable, as on the real board. With no straps, the default, the board
is polled. A strapped transmit interrupt stays on while the transmitter is idle. A parallel
input sets its interrupt on the strobe, also when nothing reads the port. A System Support 1
(`ss1`) takes the vectored interrupt lines and gives the processor the vector.

`SHOW io40` shows every unit, its ports and the live state of its lines.

### `interfacer1`: CompuPro Interfacer 1

**`interfacer1`** is the **CompuPro Interfacer 1**, the two-channel serial board of an early
CompuPro system. Each channel is a real UART with **word length, parity and stop bits that the
guest can change**, the **RS-232 modem lines**, and **receive and transmit interrupts**.

The two channels are units **`a`** and **`b`**, each with its own `[board.unit.a]` or
`[board.unit.b]` table, its own `baud` and its own `connect`. Each channel has its own pair of
ports, at any even address (`port`). By default `a` uses ports `0` and `1`, and `b` uses ports
`2` and `3`, the CompuPro standard. The data port is the lower address and the status port is
the higher one. `swap_ports = true` moves them for both channels, as the J14 jumper does.
`enabled = false` takes a channel off the bus.

#### The jumpers set the power-up state; the control port flips it

The real board has a jumper for each of eight control lines: the word format (`data_bits`,
`parity`, `parity_sense`, `stop_bits`), the two RS-232 outputs (`ca` for RTS and `cd` for DTR)
and the two interrupt enables (`rx_int_enable`, `tx_int_enable`). Each one is a property here.

An `OUT` to the status port **flips** lines away from their jumpers: a `1` bit changes that
line, and a `0` bit puts it back. Writing `00` puts every line back to its jumper, and so does
a reset. `SHOW` lists the last byte the guest wrote to the control port in `control`.

#### The status byte

D0 is TBMT (ready to send), D1 is DAV (a byte is waiting), and D2 is `opt`: the carrier input
or the UART's end-of-character. D6 and D7 are the DSR and CTS inputs. **An RS-232 input that is
ON reads 0**, as on the real board. The parity, overrun and framing error bits always read 0.

#### Interrupts

A channel's receive interrupt is on when a byte waits and `rx_int_enable` is on. Its transmit
interrupt is on when the transmitter is idle and `tx_int_enable` is on. `rx_int` and `tx_int`
send each one to a vectored interrupt line, `vi0` to `vi7`, or to `none` (the default). The
real board cannot reach the plain interrupt pin, so `int` is not a choice. A System Support 1
(`ss1`) takes the vectored interrupt lines.

## Floppy and disk controllers

The 8-inch floppy disk controllers. Several of them boot an operating system by themselves.

The built-in `default` machine has a Disk 1 but no disk image.
It starts with empty drives, and you supply the image. The disks chapter tells you how to mount
one.

### Disk timing: `timing`

Some of these controllers stop the processor with **wait states** until the disk has the next
byte. They are the `tarbell` and the `tarbelldd`.
The `timing` setting of the board sets how long the wait takes:

- **`full`** (the default): the byte is ready at once, and the wait takes no time. A seek also
  takes no time. Disk access is as fast as possible.
- **`real`**: the wait takes as long as on the real machine. A seek takes the step time of each
  track, and each byte comes at the speed of the disk. The processor loses that time, as it did
  on the real machine.

The bytes on the disk are the same with both settings. Use `real` for a guest that measures
time with the disk, or to see how fast the machine really was:

```
pascal100> SET fdc0 timing=real
```

In a machine file:

```toml
[[board]]
type   = "tarbell"
id     = "fdc0"
timing = "real"
```

With `real`, the processor must be fast enough for the disk, as on the real machine. On the
`tarbelldd`, a double-density disk without DMA needs a 4 MHz processor (`clock_hz = 4000000`). At
2 MHz, the guest loses data and reports a disk error.

### `tarbell`: Tarbell #1011 single-density floppy

The **Tarbell Electronics #1011** (July 1977) was the S-100 floppy interface that booted CP/M on
many Altair and IMSAI machines. It is a **Western Digital FD1771** soft-sector controller with
eight ports, default `F8`. It has **its own 32-byte boot PROM**, so you do not type a boot
command.

Turn the machine on with a disk in drive 0, and the machine boots itself. RESET puts the PROM at
address `0000`, over the bottom of memory. The PROM reads the first sector from the disk. When
the loaded code runs, the PROM switches off, and CP/M starts. With this board in a
machine, mount a CP/M disk in drive 0, and you get the `A>` prompt with no monitor first. With no
disk in the drive, the PROM has nothing to load, and it stops. Put a disk in and reset.

The `bootstrap` switch turns the PROM off. The board is then a plain disk controller, for a
machine that boots in a different way. It has four drives, which the software selects. The disks
are 8″ single density, with 128-byte sectors. The board recognizes a disk by the size of its
image.

### `tarbelldd`: Tarbell #2022 double-density floppy

The **#2022** (1979–80) is the #1011 with a **Western Digital FD1791**, which reads **double
density** as well as single density. It boots in the same way, with the same boot PROM and the
same `F8` ports. It boots from a **mixed-density** disk: the first track is single density, so
that the boot PROM can read it, and the other tracks are double density. The `tarbelldd` machine
boots CP/M 2.2 from a disk of this kind.

This board does everything that the `tarbell` does. It adds double density, and a **DMA
register** at port `FD`. With DMA, the board moves data to memory itself, without the processor.
Use this board for a double-density Tarbell image, and the `tarbell` for a single-density image.
The board recognizes the two kinds by the size of the image.

The board checks the density. The density bit of `OUT FC` must agree with the density of the
track, as on the hardware, and a read or a write at the wrong density gives **Record Not Found**.
A mixed-density disk needs a boot loader that sets the bit for the double-density tracks. The
two double-density CP/M disks of the Tarbell example have one. A disk of your own with an older
loader, one that reads track 1 with the bit clear, does not boot: write its boot sector again
with a loader that sets the bit.

### `disk1a`: CompuPro Disk 1A

The **CompuPro Disk 1A** (1984) is a floppy controller with an **NEC µPD765A** chip, a **DMA**
engine and a **boot EPROM**. It has four ports at `C0`–`C3`, and up to four drives. The board
moves each byte between the disk and memory itself, without the processor.

The boot EPROM holds sixteen 512-byte routines. The `routine` switch picks one, and RESET puts
it at address `0000`. Routine 0, the default, reads track 0 of drive 0 into memory at `0100`.
It then switches the EPROM off and starts the loader that it read. The `boot` switch turns the
EPROM off, for a machine that boots in a different way.

A typical machine has this board, a Z80 processor, 64K of RAM and a **System Support 1**.
The console is the serial port of the System Support 1. The machine starts the EPROM at once.
With no disk in drive 0, routine 0 tries again and again. To boot a CompuPro CP/M disk:

1. Press `Ctrl-E` to get the monitor.
2. Mount the disk in drive 0.
3. Run from `0000`.

```
pascal100> MOUNT fd0:drive0 "CCPM22.DSK"
pascal100> RUN 0
```

The board recognizes a disk by the size of its image. An 8″ CompuPro disk has a single-density
track 0, which the EPROM reads, and its other tracks can be double density. A disk can have one
side or two. A smaller image that has no known size is a blank disk. CompuPro's `FORMAT.COM`
formats it.

A blank disk needs `media`, which gives the diskette its size and sides. `8ds` is an 8″ disk with
two sides, and `5ss` is a 5.25″ disk with one side. Without `media`, the board refuses a blank,
and the error names the values to set. This command puts a blank 8″ two-sided disk in drive 1:

```
pascal100> MOUNT fd0:drive1 BLANK.DSK CREATE media=8ds
```

In a machine file:

```toml
[[board]]
id = "fd0"

  [[board.drive]]
  unit   = 1
  mount  = "BLANK.DSK"
  media  = "8ds"
  create = true
```

`create` makes the empty file `BLANK.DSK` if it is not there. The drive keeps its `media` when
`MOUNT` puts a different disk in it.

Three 5.25″ formats have 327,680 bytes, and two have 655,360 bytes. The board refuses an image
of these sizes, because the size does not tell it the format. Set `media` to the whole format,
such as `5ds-dd512` for two sides of 8 × 512-byte sectors. The error message names the values
that fit the image.

These things are not part of the board:

- **A 5.25″ disk boots from unit 2.** Routines 2 and 3 of the EPROM read it.
- **Routines 8–15 do not run.** They are for the 68000 and 32016 processors. Routines 4–7 are
  for an 8086 or 80286 (the CPU 86/87 or the CPU 286). Routines 4 and 5 can boot a DISK 3 hard disk.
- **CompuPro CP/M 2.2Q does not run.** Its BIOS needs the 8088 of the CompuPro CPU 8085/88
  board.

### `disk1`: CompuPro Disk 1

The **CompuPro Disk 1** (1981) is the board that came before the Disk 1A. It has the same
**NEC µPD765A** chip, **DMA** engine and four ports at `C0`–`C3`, and up to four drives. Port 3
is different. On the Disk 1, bit 7 of port 3 is a serial line with no UART. A guest program
sends and receives each bit itself, with a delay loop for the timing.

The boot EPROM holds eight 256-byte routines. The `routine` switch picks one, and RESET puts it
at address `0000`. The built-in EPROM is a copy of a factory EPROM. Routines 4–7 are for the
8080, 8085 and Z80, and routine 4 is the default. Each reads the start of track 0 of an 8″ disk
in drive 0 into memory at `0100`, switches the EPROM off, and starts the loader. Each routine
passes a different console code to the loaded system:

| Routine | Console of CompuPro CP/M 2.2N |
|---|---|
| 4 | The Interfacer at `00`/`01` |
| 5 | An Interfacer at `10`/`11` |
| 6 | The serial channel of the System Support 1 |
| 7 | An Interfacer 3 or 4 at `10` |

Routines 0–3 are for the 8086. Use them with the CPU 86/87 (`cpu8687`).

The built-in `default` machine has this board, a Z80 processor, 64K of RAM and an Interfacer 1
console. The console is the first serial port of the Interfacer 1, at ports `00` and `01`, as
the CompuPro CP/M 2.2N BIOS expects. To boot a CompuPro CP/M 2.2N disk:

1. Press `Ctrl-E` to get the monitor.
2. Mount the disk in drive 0.
3. Run from `0000`.

```
pascal100> MOUNT fd0:drive0 "CPM22N.DSK"
pascal100> RUN 0
```

In a machine that also has a System Support 1 (`ss1`), you can use it as the console. Set
routine 6 and move the console:

```
pascal100> SET fd0 routine=6
pascal100> CONNECT if1:a null
pascal100> CONNECT ss1:serial console
```

Each boot logs one line, `fd0: serial: a break on the line (no stop bit); nothing sent`. The
CP/M 2.2N BIOS sets bit 7 of port 3 to 1, and 1 holds the serial line at SPACING. Nothing is
connected to the line, so nothing is lost. To use the line, connect its `serial` unit:

```
pascal100> CONNECT fd0:serial telnet:2323
pascal100> SET fd0:serial baud=1200
```

Set `baud` to the rate of the guest's delay loop.

To format a blank disk, give its drive `media = "8ds"`, an 8″ disk with two sides. Without
`media`, the board refuses a blank, and the error names `8ss` and `8ds`. This command puts a
blank 8″ two-sided disk in drive 1:

```
pascal100> MOUNT fd0:drive1 BLANK.DSK CREATE media=8ds
```

In a machine file:

```toml
[[board]]
id = "fd0"

  [[board.drive]]
  unit   = 1
  mount  = "BLANK.DSK"
  media  = "8ds"
  create = true
```

`create` makes the empty file `BLANK.DSK` if it is not there. `FORMAT.COM` formats drive B
with two sides, and `STAT B:` reports a 1200K drive.

These things are not part of the board:

- **The board runs 8″ drives only.** A Disk 1 ran 5.25″ drives only after a change to the
  board's parts and wiring. The board refuses a 5.25″ disk image.
- **The M-Drive/H RAM disk is not part of the machine.** The 2.2N BIOS reports
  `0 M-Drive/H` and continues.

### `jadedd`: Jade Double D

The **Jade Double D** (Rev C, 1980) is a double-density floppy controller with its own **Z80**
processor, 2K of RAM and a **Western Digital FD1791**. The host processor never reaches the
FD1791. It sees 1K of the board's RAM through a window in its own memory, and it controls the
board through one port. The host writes a program or a command into the RAM, and the board's Z80
runs it and does the disk work. Jade's CP/M loads the board's program, DCM2, from the disk.

The board's Z80 runs on the board's own crystal: 4 MHz with 8″ drives, and 2 MHz with 5″ drives.
It runs only while the host processor runs. At the monitor, with the machine stopped, the board's
Z80 does not move.

| Setting | Values | What it does |
|---|---|---|
| `port` | `40`–`43` | The host port. The default is `43`, the port that the Jade software uses. |
| `window` | `E000`–`FC00`, on a 1K boundary | Where the host sees the board's RAM. The default is `E000`. |
| `phantom` | `true`, `false` | With `true`, the board pulls PHANTOM\* while the host reads the window. The window can then be over RAM that obeys PHANTOM\*. The default is `false`. |
| `interrupt` | `none`, `vi0`–`vi7` | The vectored interrupt line that the board pulls when its Z80 halts. The default is `none`. |
| `size` | `8`, `5` | The drive size. `8` runs the Z80 at 4 MHz, and `5` runs it at 2 MHz. The default is `8`. |

The host port has these bits:

| Bit | `OUT` | `IN` |
|---|---|---|
| D0 | `1`: the host has the RAM. `0`: the board has it. | `0`: the board's Z80 has halted. |
| D1 | Interrupt the board's Z80. While the host has the RAM, this bit also selects which 1K of the 2K is in the window. | Bit A10 of the window address. |
| D2, D3 | Not used. | Bits A11 and A12 of the window address. |
| D4–D6 | Not used. | Always `1`. |
| D7 | Reset the board's Z80, which starts at `0000`. | Always `1`. |

One `OUT` sets all three bits. `OUT 43 02` gives the RAM to the board and interrupts its Z80.
`OUT 43 80` gives the RAM to the board and starts its Z80 at `0000`. After a reset, the host has
the RAM.

`SHOW` gives the PC of the board's Z80, and whether the Z80 runs, has halted, or is stopped
because the host has the RAM:

```
pascal100> BOARDS ADD jadedd fd0
fd0: jadedd added
pascal100> MOUNT fd0:drive0 CPM22DD.DSK
fd0:drive0: mounted CPM22DD.DSK
pascal100> SET fd0 interrupt=vi2
pascal100> SHOW fd0
```

`SHOW fd0` prints this line after the settings:

```
  board Z80  PC 0000  stopped  RAM held by the host
```

The board has up to four drives. It recognizes a disk by the size of the image:

| Image size | Disk |
|---|---|
| 489,728 bytes | A Jade double-density disk. Track 0 is single density, and tracks 1–76 are double density, with 50 sectors. |
| 259,328 bytes | A Jade single-density disk. Track 0 and tracks 2–76 are single density. Track 1, the system track, is double density. |
| 256,256 bytes | An IBM 3740 single-density disk. |
| Smaller | A blank disk. The guest can format it. |

The board refuses an image of any other size. The board checks the density. A read at the wrong
density gives **Record Not Found**, as on the hardware.

To boot Jade CP/M 2.2, a machine needs a Z80 board, 56K of RAM, an 88-2SIO at port `10`, this
board at port `43` with its window at `E000`, and a boot PROM, **DDBOOT-2SIO**
(`builtin:ddboot-2sio`), at `F000`. pascal100 has no built-in machine for it, so write a machine
file with these boards. The PROM is in the
host's memory, not on the board. It starts the board's Z80, which reads CP/M from drive 0. The CP/M BIOS on Jade's
disks uses the PROM's console routines, so the PROM stays in memory while CP/M runs.

The console is channel A of the 88-2SIO at port `10`. Jade's own PROM, `builtin:ddboot`, reads a
console at ports `00` and `01`. Jade marked those ports as the places to change for the owner's
console board. `builtin:ddboot-2sio` is Jade's PROM, changed at those places for the 88-2SIO. No
other byte of the PROM is different. To boot CP/M:

1. Start your Jade machine file, for example `pascal100 jadedd.toml`. With no disk in drive 0,
   the PROM prints `INSERT SYSTEM DISKETTE` and waits.
2. Press `Ctrl-E` to go to the monitor.
3. Type `MOUNT fd0:drive0 JADE20K.DSK`, with the name of your Jade system disk.
4. Type `RUN`. The PROM reads the drive again and boots CP/M:

```
JADE COMPUTER PRODUCTS
20K CP/M 2.2 DDBIOS2


A>
```

A disk with a 56K system prints `56K CP/M 2.2 DDBIOS2`.

Jade's `FORMAT` program makes a new disk. The CP/M BIOS on the 56K disk has two drives, A: and
B:, so put the new disk in drive B. `FORMAT` reads each answer as a line, so press Return after
each one. To make a double-density disk that boots:

1. Press `Ctrl-E` to go to the monitor.
2. Type `MOUNT fd0:drive1 NEW.DSK CREATE`, with the name of the new disk.
3. Type `RUN` to go back to CP/M.
4. Type `FORMAT`.
5. Type `B` at `Select DRIVE to be FORMATTED:`.
6. Type `1` at `ENTER Function NUMBER:`. `FORMAT` formats the disk and asks for the next function.
7. Type `4`, then `A` at `READ System from DRIVE:`. `FORMAT` reads the system tracks of drive A.
8. Type `5`. `FORMAT` writes the system tracks to drive B.
9. Press `Ctrl-C` to go back to `A>`.

Function `2` makes a Jade single-density disk, and function `3` makes an IBM 3740 disk. `FORMAT`
can also reformat a disk to a different layout. The image file then changes to the size of the
new layout, so the next `MOUNT` reads the disk correctly.

These things are not part of the board:

- **The serial port.** The board's EIA output is not connected. DCM2 sends list output there, and
  that output is lost.
- **Two-sided disks.** Each drive has one side.
- **The 24-bit address mode.** The window is in the 16-bit address space.
- **The debugger on the board's Z80.** `BREAK`, `STEP` and `REGS` work on the host processor only.

### `mdriveh`: CompuPro M-Drive/H

The **CompuPro M-Drive/H** (1982) is 512K of RAM that the system uses as a fast disk drive. It
uses no memory addresses. The processor sees two ports: `C7` loads an address, and `C6` reads or
writes the byte at that address. After each byte the address goes up by one, so a sector needs
one address and then 128 bytes. The data is lost when the power goes off.

CompuPro's CP/M 2.2, CP/M-86 and MP/M 8-16 have the driver built in. At a cold boot they count
the boards, show the size, and format **drive M:**. Then you use M: as any other drive. Copy your
files back to a disk before you stop the simulator, because nothing saves M:.

```
CP/M 86 1.1PAA
...
512K M-DRIVE/H Active
Total Memory: 1 MEGABYTE
...
FORMATTING MEMORY DRIVE

A>PIP M:=A:*.CMD[V]
```

To add one to a machine file:

```toml
[[board]]
type = "mdriveh"
id   = "mdh0"
```

| Property | Default | What it does |
|---|---|---|
| `port` | `C6` | the data port; the address port is the next one. An even number |
| `cascade` | `0` | which board this is, 0-7. Give each board in the machine its own number |
| `fill` | `random` | what the RAM holds at power-on: `random` or `zero` |
| `seed` | `1` | the seed for `fill = random` |

**More than one board.** Each board holds 512K. Put up to eight boards at the same `port`, with
`cascade` 0, 1, 2 and so on. The system sees one drive: 1024K with two boards, up to 4M with
eight. Two boards with the same `cascade` both answer a read, and `SHOW BUS CONTENTION` reports
it.

RESET does not clear the RAM. A power cycle fills it again per `fill`, as the real chips lose
it.

Not modeled: the wait states of the real board, and the power-fail jumper J1.

## Interrupts and the clock

Vectored interrupts and real-time clocks.

### `ss1`: CompuPro System Support 1

A **multifunction** board. On the real board, one block of 16 ports holds a serial channel, an
interval timer, two interrupt controllers, a battery-backed **real-time clock and calendar**,
and a socket for a math coprocessor. This board models the **clock**, the **serial channel**,
the **interval timer**, the **two interrupt controllers** and the **math socket**. The math
socket is empty by default.

The clock is an OKI MSM5832. It starts with **the date and time of your computer**, so a guest
program that reads it gets the real time. A guest can also **set** it. After that, the clock
keeps the setting, and the time is kept through a RESET, as the battery on the real chip did.
The block is at base **`50H`** by default, the CompuPro convention. The `base` property moves
it.

The serial channel is a **2651 UART** at `5C`–`5F`. It is a spare serial port. A guest sets its
rate, its frame and its RS-232 handshake through the mode and command registers. You connect it
with `CONNECT ss1:serial <endpoint>`. Its `baud`, `interrupt` and `connect` are unit settings,
which `SHOW ss1` shows.

The interval timer is an **Intel 8253** at `54`–`57`. It has three independent 16-bit counters,
which a guest can use as timers or as square-wave and rate generators. The counters count at 2
MHz, and their outputs go to the interrupt controllers. `SHOW ss1` prints a live `timer` line
with the mode, count and output of each counter.

The interrupt system is two **Intel 8259A** controllers at `50`–`53`, cascaded as master and
slave:

- The master watches seven vectored interrupt lines of the bus, VI0 to VI6, and drives the
  processor's interrupt pin.
- The slave watches VI7 and the sources on the board. These are the three timer outputs, the
  math chip, and the UART's transmit-ready and receive-ready signals. The slave sends them to
  the master.

A guest sets them up in the usual 8259A way, with the ICW and OCW words, and unmasks the sources
that it wants. On an interrupt, the controller gives the processor a `CALL` to the vector of the
source with the highest priority. `SHOW ss1` prints a live `pic` line with the request,
in-service and mask registers of each controller. The master is the interrupt priority encoder
of the machine.

The math socket takes an **AMD 9511A** (Intel 8231A) or an **AMD 9512** (Intel 8232). The
`math` property selects the chip: `none` (the default), `9511a` or `9512`. With no chip, the
two ports read `FF`, as on a stock board. The chip is at `58` (data) and `59` (command and
status). A guest pushes the operands, writes a command and pops the result.

- The **9511A** does 16-bit and 32-bit fixed point, 32-bit floating point, and the functions
  SQRT, SIN, COS, TAN, ASIN, ACOS, ATAN, LOG, LN, EXP and PWR.
- The **9512** does 32-bit and 64-bit floating point in the format of the proposed IEEE standard:
  add, subtract, multiply and divide.

Each command takes the number of cycles that the data sheet gives, at 2 MHz. If the guest uses the data
port or writes a command before a command ends, the board holds the processor in wait states
until the command ends. The chip's END signal goes to slave IR5 and its SVREQ signal goes to slave IR4,
so a guest can also wait for an interrupt. `SHOW ss1` prints a live `mathchip` line with the
state of the chip, its status and the top of its stack.

```
pascal100> SET ss1 math=9511a
```

In a machine file:

```toml
[[board]]
type = "ss1"
id   = "ss1"
math = "9511a"
```

A Disk 1A machine often has one, with its console on the serial channel of the board. The clock is
at `5A` (command) and `5B` (data). The board reference gives the
digit map, the read and set sequences and the register layouts.

## Video

### `pcvideo`: CompuPro PC Video

The CompuPro PC Video is a color display board with a keyboard port. It answers at the
addresses of IBM's Color Graphics Adapter: 16K of screen RAM at `0B8000` and 16 ports at
`03D0`, decoded on all 16 address bits. Its picture opens in a window: 80x25 or 40x25 text, or
320x200 or 640x200 graphics, in 16 colors. Only the color half of the board is built.

The screen RAM must not belong to a RAM board as well. Leave a hole in the RAM at
`0B8000`-`0BBFFF`:

```toml
[[board]]
type = "memory"
id   = "mem0"
ext  = true
honors_phantom = "read"
  [[board.region]]
  type = "ram"
  at   = 000000
  size = "736K"
  [[board.region]]
  type = "ram"
  at   = 0BC000
  size = "272K"

[[board]]
type = "pcvideo"
id   = "pcv0"
  [board.unit.kbd]
  connect = "null"
```

**The keyboard.** The board's window is an IBM PC keyboard: click the window and type. Each
key down and each key up reaches the guest, Shift, Ctrl and Alt (Option on a Mac) alone
included. The keyboard is the 83-key PC/XT keyboard, so the arrows, Home, End, PgUp, PgDn,
Insert and Delete send the keypad's codes, and F11, F12 and Cmd send nothing. A paste (Cmd-V,
or Ctrl-Shift-V) is typed as characters. Ctrl-E in the window goes to the guest: use the
terminal's Ctrl-E or the window's close box to stop it.

The unit `kbd` is a second keyboard, for a script. With `keys = ascii` (the default), each byte
that reaches `kbd` is a character, and the board types it as the keyboard would, with Shift or
Ctrl. With `keys = scancodes`, each byte is a scan code. Send them with the MCP `send` tool or
through an endpoint such as `connect = "socket:2400"`.

**The light pen.** `SET pcv0 lightpen=mouse` makes the mouse a light pen: hold the left button
over the window. The pen sees only lit dots.

**The window** has a title bar, as a terminal window has. `title` sets its text (empty:
`CompuPro PC Video (pcv0)`), and `titlebar = false` takes it away.

**The character generator** is `builtin:pcvideo`, a public-domain font in the layout of the
board's PROM. To use a dump of a real PROM, give its file in `font`.

## Host integration

A board that connects the machine to your computer. It is not period hardware.

### `hostbridge`: our own board, not a period board

**This board never existed.** It is the one part of the machine that is not history, and this
manual says so.

It moves **files between the guest and your computer**, in both directions. It is **sandboxed**.
The guest sees one folder that you choose, and **it cannot leave that folder**. It cannot use
`..` or an absolute path to leave it. This is a fixed rule, not a setting.

The default port is `B0`. **The utilities are not in the board.** `R`, `W` and `HDIR` are
ordinary CP/M `.COM` programs. They are on a disk, they run at the `A>` prompt, and they use the
two ports of the board, as any other CP/M program could. They were assembled *in the machine*,
with the machine's own assembler, from 8080 source that ships with the package. You can read
their code.

The file-transfer chapter describes this board. It is the fastest way to get your own code into
CP/M. XMODEM also works, over an ordinary serial line, as it did on real machines.
