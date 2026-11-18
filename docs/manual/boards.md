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
pascal100> BOARDS ADD rtc100 rtc0
pascal100> SET rtc0 port=E0
pascal100> SHOW rtc0
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

## The boards

The boards are in groups, in the same order as the sections below.

**Memory**

| Type | What it is |
|---|---|
| `memory` | RAM and ROM, as a list of regions |
| `bankmem` | bank-switched RAM: Vector Graphic, Cromemco 64KZ, North Star HRAM, ExpandoRAM II and III |

**Processors**

| Type | What it is |
|---|---|
| `8085` | an 8085 processor board. It runs all 8080 code, and adds RIM, SIM and the TRAP and RST interrupts |
| `z80` | a Z80 processor board. The same bus, with a different instruction set |
| `pascal100` | Digicomp Research Pascal-100: a Z80 and the Western Digital MicroEngine, one active at a time |

**Serial ports and consoles**

| Type | What it is |
|---|---|
| `2sio` | MITS 88-2SIO: two serial ports |
| `gsio` | a generic serial board with two channels, set up with straps |
| `io4` | SSM IO-4: two serial ports and four parallel ports |
| `interfacer1` | CompuPro Interfacer 1: two serial ports with a control port and interrupts. The console of the CompuPro machines |

**Printers**

| Type | What it is |
|---|---|
| `c700` | MITS 88-C700: the line-printer controller. Capture to a file |

**Floppy and disk controllers**

| Type | What it is |
|---|---|
| `tarbell` | Tarbell #1011: a single-density floppy controller with its own boot PROM. Boots CP/M by itself |
| `tarbelldd` | Tarbell #2022: the double-density version, for mixed-density disks |
| `disk1` | CompuPro Disk 1: an 8″ floppy controller with DMA, its own boot EPROM and a bit-banged serial line. Boots CompuPro CP/M 2.2 and UCSD Pascal on the Pascal-100 |
| `disk1a` | CompuPro Disk 1A: an 8″ floppy controller with DMA and its own boot EPROM. Boots CompuPro CP/M |
| `jadedd` | Jade Double D: a double-density floppy controller with its own Z80. The host loads the program that the Z80 runs |

**Interrupts and the clock**

| Type | What it is |
|---|---|
| `ss1` | CompuPro System Support 1: a real-time clock, a serial channel, an interval timer, two interrupt controllers and a math socket |
| `rtc100` | SciTronics RTC-100: a battery-backed real-time clock, with an optional interrupt each second |

**Host integration**

| Type | What it is |
|---|---|
| `hostbridge` | file transfer to your computer. **Our own board, not a period board** |

**PROM programmers**

| Type | What it is |
|---|---|
| `pb1` | SSM PB1: program a 2708 or 2716 EPROM, and save it as a hex file |

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

A plain `memory` board has no bank switching. Bank switching is a different board, `bankmem`.

### `bankmem`: bank-switched RAM

When 64K was not enough, S-100 makers put several planes of RAM at the same addresses. A
write-only **select port** chose which plane the processor saw. Each maker did this in a
different way, so `bankmem` is **one board with five decoders**. Its `card` property selects the
decoder:

| `card` | Real board | Select port | What a write does |
|---|---|---|---|
| `vector` | Vector Graphic 64K | `40` | **one bit for each bank**: `01`→bank 0, `02`→1, `04`→2 … `80`→7 |
| `cromemco64kz` | Cromemco 64KZ / 64KZ-II | `40` | **8-bit mask**: bit *N* turns bank *N* on. **Several banks can be on** (`28`→banks 3 and 5) |
| `northstar` | North Star HRAM | `C0` | bit 0 = on or off, bits 1–7 = which bank. Banks change **one at a time** |
| `expandoram2` | SD Systems ExpandoRAM II | `FF` | the low four bits are a **page number** (see the note below) |
| `expandoram3` | SD Systems ExpandoRAM III | `FF` | the low four bits are a **page number** (see *The ExpandoRAM III* below) |

`banks` sets the number of planes on the board: up to 8 for `vector` and `cromemco64kz`, 6 for
`northstar`, and 10 for `expandoram2` (11 with a partition). On `expandoram3`, `ram` sets the size instead. `fill` and `seed` work as they do on `memory`. The guest
writes the select port. At the monitor, you can write it yourself with `OUT`, and `SHOW` lists
every plane and which plane is on.

On `vector`, a select of a bank that the board does not carry takes the live bank off the bus, and
nothing answers until a bank that exists is selected. The `window_lo` and `window_hi` properties set
the addresses of each plane on `vector`. The default is `0000` to `FFFF`. The real board ships with
its last address at `DFFF`, which leaves room for a ROM and a video board.

The package has no operating system that uses banks. The quickest way to see the board work is
from the monitor:

```
$ pascal100 bankmem
pascal100> OUT 40 01
port 40 <- 01
pascal100> DEPOSIT 1000 A0
pascal100> OUT 40 08
port 40 <- 08
pascal100> DEPOSIT 1000 B3
pascal100> OUT 40 01
port 40 <- 01
pascal100> DUMP 1000-1000
1000  A0                                                .
pascal100> OUT 40 08
port 40 <- 08
pascal100> DUMP 1000-1000
1000  B3                                                .
```

`OUT 40 08` selects bank 3, because bit 3 is on. It does not select bank 8. The two `DUMP`
commands read different bytes at the same address, because the plane changed.

> **The `expandoram2` pages.** The board uses only the low four bits of the byte, so `OUT FF 13`
> selects page 3. A PROM on the board makes each page from 16K banks on several boards.
> `bankmem` shows the guest the same pages, but it does not model each board. The two stock
> PROMs set a partition:
>
> - `partition = "ex48"` gives 48K pages at `0000`–`BFFF` and 16K of common RAM at `C000`.
>   Page `A` is only 16K, at `8000`–`BFFF`.
> - `partition = "ex32"` gives 32K pages at `0000`–`7FFF` and 32K of common RAM at `8000`.
>
> `ram` sets the RAM, and the program calculates the number of pages from it. With a partition,
> `ram` counts 64K boards of 16K chips, because the PROM maps are for those boards. For example,
> `ram = 256` with `ex48` is four boards, with pages 0–4. `ram = 512` with `ex48` is eight
> boards, with pages 0–9 and `A`. When the guest selects a page that has no RAM, nothing answers
> in the addresses of the page, and the common RAM continues to answer.
>
> With `partition = "none"` (the default), each page is a whole 64K plane. `ram = 256` is one
> board of 64K chips, with four pages. The ExpandoRAM II manual gives no page map for that
> board, so this setting is an approximation.
> The `vector`, `cromemco64kz`, `northstar` and `expandoram3` cards are exact.

#### The ExpandoRAM III

`card = "expandoram3"` models the PROM map of the SD Systems ExpandoRAM III. The board uses only
the low four bits of the byte that the guest writes to port `FF`. For example, `OUT FF 12`
selects page 2. Each board holds 256K of RAM in 16K cells. For each page, the PROM on the board
connects one cell to each 16K of the address space. These settings are the switches on the board:

| Setting | Values | What it does |
|---|---|---|
| `ram` | `256`, `512` | `256` is one board, with pages 0–4. `512` is two boards, with pages 0–9. |
| `config` | `cpm`, `oasis` | Switch 5 of S3. `cpm` (the default) puts 48K pages at `0000`–`BFFF` and 16K of common RAM at `C000`. `oasis` puts the common RAM at `0000` and the pages at `4000`–`FFFF`. |
| `bank_enable` | `00`–`FF` | Switches 1–4 of S3 on each board. Bit *board* × 4 + *bank* is on when that bank is on. The default `FF` turns every bank on. A bank that is off answers nothing. |

The common RAM answers for every page. A page that has no cells on the boards has no RAM. Page 7
on one board and pages `A`–`F` are examples. When the guest selects such a page, nothing answers
in the addresses of the page, and the common RAM continues to answer. `partition` is refused on this
card, because the PROM sets the map.

A banked CP/M BIOS for the ExpandoRAM II with `partition = "ex48"` also runs on the ExpandoRAM
III with `config = "cpm"`. Both boards write the page number to port `FF`, and both put 48K pages
under 16K of common RAM at `C000`.

The ExpandoRAM III manual prints some cells of its page map incorrectly. `bankmem` corrects them.
The corrections change what the guest sees only when a bank is off.

```
$ pascal100 bankmem --set "mem0 card=expandoram3"
set> SET mem0 card=expandoram3
mem0: card=expandoram3
pascal100> OUT FF 01
port FF <- 01
pascal100> DEPOSIT 1000 A1
pascal100> DEPOSIT C000 CC
pascal100> OUT FF 12
port FF <- 12
pascal100> DEPOSIT 1000 A2
pascal100> OUT FF 01
port FF <- 01
pascal100> DUMP 1000-1000
1000  A1                                                .
pascal100> DUMP C000-C000
C000  CC                                                .
pascal100> OUT FF 02
port FF <- 02
pascal100> DUMP 1000-1000
1000  A2                                                .
pascal100> DUMP C000-C000
C000  CC                                                .
pascal100> OUT FF 07
port FF <- 07
bank: page 7 has no RAM on expandoram3 (mem0, pages 0-4). Nothing answers 0000-BFFF; common still does.
```

`OUT FF 12` selects page 2, because the board ignores the high four bits. Address `1000` holds
`A1` on page 1 and `A2` on page 2. Address `C000` is common RAM, so it holds `CC` on every page.

## The processor boards

The processor boards are `8085`, `z80` and `pascal100`. This section applies to all of them.
The sections after it describe each one.

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

### `gsio`: the generic serial board, set up with straps

Most serial boards in this chapter model **one specific chip**, such as the 2SIO's 6850, the
SBC's 8251 or the 88-SIO's COM2502. Software must be written for that chip. **`gsio`** is
different. It models no specific chip. You **describe** the port with straps:

- which port is status and control, and which port is data
- which status bit means that a byte is waiting (`dav`)
- which status bit means that the transmitter is ready (`tbmt`)
- whether the shared **`inverter_gate`** inverts both bits

Use it for any polled interface that reads a status bit and then a data byte.

The board has **two independent serial channels**, units **`a`** and **`b`**. Each channel has
its own straps, `baud` and `connect`, in its own `[board.unit.a]` or `[board.unit.b]` table. By
default, `a` uses ports `0` and `1`, and `b` uses ports `2` and `3`.

You seldom set the straps one at a time. A **profile** sets them to match a known board:

- **`sior1`**: MITS SIO Rev 1. This is the default, and the SSM 8080 monitor expects it
- `sior0`: MITS SIO Rev 0
- `tuart`: Cromemco TU-ART
- `imsai-sio2`: IMSAI SIO-2
- `compupro-if2`: CompuPro Interfacer II
- `compupro-ss1`: CompuPro System Support 1

Select a profile for each channel, and then change any strap, as you would move a jumper. The
board is polled, with no interrupts. It **only sends and receives bytes**. It does not model
word length, parity or stop bits. A board that needs those is a separate board with a full
model.

### `io4`: SSM IO-4 (2P + 2S)

`gsio` describes a serial port with straps. **`io4`** is a specific board, the **SSM IO-4**. It
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
guest can change**, the **RS-232 modem lines**, and **receive and transmit interrupts**. Use it
in place of the `gsio` console when a program needs more than "send and receive a byte".

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

## Printers

Line-printer controllers. Each one sends its output to a file or to a print queue.

### `c700`: MITS 88-C700

The **line-printer controller**, an output-only board for an Altair C700 printer. It has unit
`prn` and default port `02`, which is the MITS default: control and status at `02`, data at
`03`.

The package has no printer, so **`CONNECT` the `prn` line to where you want the output**:

- a file: `CONNECT lpt0:prn out:printout.txt`
- the `console`, to see it print while it runs
- a `socket:`
- a real `serial:` printer
- a **real print queue on your computer**: `CONNECT lpt0:prn printer:linewriter`. This works
  when your build found a print system. The serial chapter gives the options.

The output is the exact bytes that the program sent, with the control codes. It is not a
formatted page.

You can use the board **polled or with interrupts**:

- **Polled:** write a character to the data port (`03`). Before the next character, read the
  status port (`02`) until bit 0, ACKNOWLEDGE, is set.
- **Interrupts:** set control bit 1 to turn on its **single-level interrupt**. The printer then
  interrupts the processor each time it takes a byte, and the handler sends the next byte.

The `interrupt` strap selects the line: pin 73 (`int`, the default), a vectored interrupt level
(`vi0` to `vi7`), or `none`. `interrupt_after` is switch SW2 #4 of the board. It sets whether
the interrupt comes after every character, or only after a CR or LF.

In the CompuPro machines, ports `02` and `03` belong to channel B of the Interfacer 1, so give
the board another port when you add it: `BOARDS ADD c700 lpt0 port=04`. Its output starts on
`null`. To capture it, type `CONNECT lpt0:prn out:printout.txt`. To see it, connect it to
`console`, which takes the console from the terminal.

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
- **Only routines 0–3 run.** The other routines are for the 8086, 68000 and 32016 processors.
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

Routines 0–3 are for the 8086 and do not run here.

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

### `rtc100`: SciTronics RTC-100

A **battery-backed calendar clock**, and nothing else. SciTronics sold it in 1980 for machines
that did not know the date. It has an OKI MSM5832 clock chip, the same chip as on the System
Support 1, behind a 6821 parallel interface, on four ports in a row.

It starts with **the date and time of your computer**, so a guest gets the real time. When a
guest sets it, the clock keeps that setting through a RESET and a power cycle, as the lithium
cell of the real board did. `SHOW <id>` prints a live `time` line with the time on the clock and
how far it is from the time of your computer.

The base address is the PORT switch of the board, and it **must be a multiple of 4**. The switch
decodes only the top six address bits, and the bottom two bits select one of the four ports. The
default is `F0`. A guest reads the clock one BCD digit at a time. It writes the number of the
digit to the first port, and reads the digit back **in the top half of the byte**. For this
reason, the period drivers for this board all mask and rotate the byte at the end.

It can also **interrupt once a second**, to drive a clock display in the background. Set
`interrupt` to `int`, and the board pulls the interrupt line of the machine. `restart` selects
the `RST` that it gives the processor (0 to 7, the INT switch of the board). The manual of the
board says that 0 and 7 are often used by other things. With `interrupt` set to `none`, the
board never interrupts, and it still keeps the correct time.

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

## PROM programmers

A board that does not run software, but *programs* it into a chip. You load a period EPROM
programmer routine and run it. The bytes that it programs go into a socket, and you can save the
socket to a file.

### `pb1`: SSM PB1

The **SSM PB1** (Solid State Music) is a 2708 and 2716 EPROM programmer. It has a **4K window**
in memory (default `D000`) for its two sockets: **U22** holds a 2708 (1K), and **U23** holds a
2716 (2K). It has one **control port** (default `10`). Port `10` is free in the CompuPro
machines, but their 64K of RAM covers the window, so a machine with a `pb1` needs less RAM, as in
the example below.

Software programs the chip:

1. An `OUT` to the control port arms the board and selects the chip. Write `01` for the 2708, or
   `02` for the 2716.
2. After that, every byte that the guest **writes into the window** is programmed into the chip.
3. A **read** of the window disarms the board again. The period routines end in this way, and it
   turns the LED off.

"Programming is a write, and disarming is a read" has two results:

- **A socket starts erased**, with every byte `FF`. **Programming can only clear bits to 0.** It
  cannot set them, as on a real EPROM. Program a *blank* chip. A second program over the first
  gives the AND of the two.
- The program is in the board, not on your computer. To keep it, **`SAVE` it to a file**
  (below). A power cycle reads the sockets again from their mounts, so a program that you did
  not save is lost.

#### Programming an EPROM, and making a hex file

You need this board, not only a write to memory, because you want to run the period *software*.
The PB1 manual has SSM's programmer routines, one for the 2708 and one for the 2716. Each
routine copies bytes from `4000` into the socket. The 2708 routine starts at `0100`. The package
does not include these routines. Enter one from the manual, or `LOAD` it from a hex file of your
own. After that, put the data at `4000`, run the routine, and save the result:

```
pascal100> FILL 4000-43FF E5
pascal100> LOAD PROG2708.HEX
pascal100> BREAK F021
pascal100> RUN 100
pascal100> SAVE eprom.hex D000-D3FF
```

- `FILL` puts the 1K that you want to program at `4000`. You can also `LOAD` your own data
  there.
- `LOAD PROG2708.HEX` loads the 2708 routine.
- `BREAK F021` is needed because SSM's routine ends with a jump to SSM's own system monitor, at
  `F021`. A machine without that monitor has nothing at `F021`. The breakpoint stops the
  machine at that jump, after the chip is programmed. The next section shows a machine that
  has the monitor, and needs no breakpoint.
- `RUN 100` runs the routine. It arms the board and programs the 2708.
- `SAVE` reads the socket window from the bus and writes it to a file. After it is programmed,
  the socket reads like other memory. `eprom.hex` is an ordinary Intel HEX image of the chip.
  You can give it to another tool, or `LOAD` it again later.

Your own routine works in the same way. It writes `01` to port `10` (or `02` for a 2716), and
then writes the data into the window. After it runs, `SAVE` the window.

#### With the SSM monitor

SSM wrote the routines for the **SSM 8080 System Monitor**. Each routine ends with a jump to
the monitor at `F021`. The manual also has two check routines, which print `P` (pass) or `F`
(fail) through the monitor's console-output routine at `F009`. The package has that monitor as
the built-in ROM `ssm-8080mon`. Put it in the machine, and the routines run as SSM wrote them.

The monitor has its console on a serial port at `00` and `01`, as an `io4` has by default. Thus
this machine has an `io4` as its console, RAM below the window of the `pb1`, and the monitor at
`F000`:

```toml
[machine]
name = "pb1"

[[board]]
type = "z80"
id   = "cpu0"

[[board]]
type = "io4"
id   = "io40"

  [board.unit.a]
  connect = "console"

[[board]]
type   = "pb1"
id     = "pb1"
port   = 10
window = D000

[[board]]
type = "memory"
id   = "mem0"

  [[board.region]]
  type = "ram"
  at   = 0000
  size = "52K"

  [[board.region]]
  type  = "rom"
  at    = F000
  mount = "builtin:ssm-8080mon"
```

Put the data and the routine in memory as before, and then start the monitor:

```
pascal100> FILL 4000-43FF E5
pascal100> LOAD PROG2708.HEX
pascal100> RUN F000
```

The monitor prints its name and its prompt, which is a period:

```
MONITOR V1.0
.
```

Type `G100` and press Enter. `G` is the monitor's command to run a program. The routine programs
the chip and then jumps to `F021`, and the monitor prints its name and its prompt again:

```
.G100

MONITOR V1.0
.
```

Press `Ctrl-E` to go back to the `pascal100>` prompt, and `SAVE` the window as before.

A check routine runs in the same way, with `G` and its start address. It prints its result
before the monitor prints its name. This is the manual's copy-verify routine, which starts at
`0180`, after a good burn:

```
.G180
P
MONITOR V1.0
.
```

The board also has an optional **EPROM area on the board** (U11 to U14 on the real board). These
are read-only chips that you mount above `8000` with `[[board.prom]]` (`at` and `mount`). Use
them as the *source* when you copy one EPROM to another.

