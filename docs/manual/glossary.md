# Glossary

**ACIA**: Asynchronous Communications Interface Adapter. The Motorola 6850, a serial chip used
on boards such as the MITS 88-2SIO. A program sees two of its registers: a status register and a
data register.

**ATTN**: See **STOP**. ATTN is the old name of the STOP key. `CONSOLE attn=` still works, and
it means the same as `CONSOLE stop=`.

**backplane**: The circuit board with the connectors that every board plugs into. It is not one
of the boards. A machine file adds no backplane, because a machine file describes what is in the
backplane. On the machines here, the backplane is the S-100 bus: slots of one hundred pins each,
all connected in parallel. There is no chipset. The backplane *is* the machine.

**bank switching**: A way to give the processor more memory than it can address. Several banks
of memory use the same addresses, and a switch selects one bank at a time. A Z80 can address
64K and no more, so a machine with 128K of RAM shows the processor one part of it at a time.
The Pascal-100 does this with its 128K: 16 pages of 8K.

**BDOS**: Basic Disk Operating System. The middle layer of CP/M, which handles files,
directories and the console. A CP/M program asks the BDOS for what it needs, and does not know
which hardware answers.

**boot EPROM**: The EPROM on the CompuPro Disk 1 that starts the machine. It answers at `0000`
over PHANTOM\*, reads the start of track 0 into memory, and jumps to it. The guest switches it
off with `OUT C3` and D0 = 0. **There is no `BOOT` command.** `RUN 0` starts the boot EPROM, as
a reset did on the real machine.

**BIOS**: Basic Input/Output System. The bottom layer of CP/M, and the only part that knows what
machine it is on. To move CP/M to a new computer, you rewrite the BIOS. The BDOS and the CCP do
not change. A track buffer is also in the BIOS, *if the author put one there*. For this reason,
on some versions of CP/M and not on others, a file can be "written" and not yet be on the disk.
See the disks chapter.

**board**: Anything that plugs into the backplane: memory, a serial port, a disk controller, or
the processor itself. This program uses the word everywhere. `BOARDS` lists
them, and `[[board]]` adds one in a machine file. A board is the thing that you add, remove,
`SHOW` and `SET`. See also **card**, which is the same object.

**card**: The same object as a **board**. The period hardware and its manuals said "card". This
manual uses the word only when the sentence is about the physical object that somebody bought,
put chips into and set jumpers on. Everywhere else, it says board.

**CCP**: Console Command Processor. The top layer of CP/M, which prints `A>` and runs what you
type. A large program can write over it, and CP/M loads it again from the disk afterward. That
load is the warm boot.

**contention**: Two boards that answer the same address. On a real S-100 machine, both would
drive the data bus at the same time, and you would get a byte that is neither board's value,
sometimes and not always. That is very hard to find. `SHOW BUS CONTENTION` names the boards.

**CP/M**: Control Program for Microcomputers. The disk operating system of Digital Research, and
the reason that the 8080 was important. A program written for CP/M ran on 8080 and Z80 machines
from many different makers.

**decode**: What a board does when it recognizes an address as its own, and answers. A board
that does not decode an address stays silent, and another board can answer. Which board decodes
which address is the main question of how an S-100 machine is put together.

**DMA**: Direct Memory Access. A board takes the bus from the processor and reads or writes
memory itself, without the processor. It is faster, and it can cause faults that are difficult
to find.

**endpoint**: In this program, the thing at the far end of a serial unit's cable, for example
`console`, `null`, a TCP socket or a real serial port. The serial chapter has the complete list.

**floating bus**: What the data bus reads when nothing drives it. On the S-100 bus it floats
high, so an `IN` from a port that no board decodes returns `FF`. This is not an error. It means
that no board is there.

**guest**: The software that runs on the simulated machine, such as CP/M, UCSD Pascal or a program
that you wrote. The guest cannot tell that the machine is simulated.

**hard-sector**: A floppy disk where physical holes in the disk mark the sector boundaries, and
the controller counts them as they go past. The MITS floppy disks are hard-sectored. A
soft-sectored disk has one hole, and finds its sectors from marks written in the data. Soft
sectors became the common method. The CompuPro Disk 1 uses soft-sectored disks.

**IEEE 696**: IEEE Std 696-1983, the standard version of the S-100 bus. It has 24 address
lines, a 16-bit I/O address and 16-bit memory and I/O transfers. It is the only bus in this program. The
configuring chapter tells you more.

**IntAck**: Interrupt Acknowledge. The bus cycle that the processor runs when it accepts an
interrupt. The board that interrupts puts an instruction or a vector on the data bus during that
cycle, and the processor uses it.

**machine file**: The `.toml` file that describes a machine: its boards, their settings, and the
commands to run when it starts. The configuring chapter describes the format.

**MCP**: Model Context Protocol. The protocol that an AI assistant uses to call structured
tools. `pascal100 --mcp` uses it, so that an assistant can control the machine directly. See the
MCP chapter.

**monitor**: The `pascal100>` prompt. It is not a program that runs in the machine. It is you,
in front of the machine, with more controls than a real front panel had.

**PHANTOM\***: An S-100 line that tells memory boards to stop answering. When a board pulls it,
a ROM can answer at an address that a RAM board also decodes, with no contention. In this way, a
boot PROM can cover RAM and then get out of the way. The asterisk means that the line is active
low, as most lines on this bus are.

**Pascal-100**: The Digicomp Research Pascal-100, an S-100 processor card with a Z80 and a
Western Digital MicroEngine. One of the two processors runs at a time. The MicroEngine runs UCSD
Pascal p-code directly. The boards chapter tells you more.

**pINT**: The S-100 interrupt request line. Any board can pull it. The processor sees it, if
interrupts are enabled, and runs an IntAck cycle to find out which board interrupted and why.

**PROM**: Programmable Read-Only Memory. A chip with a program in it that stays when the power
is off. The boot loader of a disk machine is in one. Without it, the machine could not start,
because something must already be in memory to read the disk. See **boot EPROM**.

**RST**: Restart. A one-byte 8080 and Z80 instruction that calls a fixed low address: `RST 0` to
`RST 7`, at `0000`, `0008` and so on to `0038`. It is one byte, so it fits in an IntAck cycle.
That is what it is for.

**S-100**: The bus. One hundred pins. MITS designed it for the Altair 8800 in 1975, and then
other makers used it. IEEE 696 is its later standard version. It was an early open standard in personal computing, and for this
reason, a board from one company worked in the machine of another company.

**sector**: The smallest part of a disk that you can read or write. On a Disk 1 floppy, a sector
has 128, 256, 512 or 1024 bytes. Track 0, side 0 always has 128-byte sectors.

**STOP**: The key that stops the machine, `Ctrl-E` by default. The name comes from the STOP
switch on a front panel. It stops the processor and gives you the monitor. It does not change the machine. It is not
RESET, and it is not POWER. For this reason, `RUN` with no address continues at the instruction
that the machine was about to execute. The program reads the key before the guest sees the byte,
so no guest program can take it from you. Move it with `CONSOLE stop=`. The older `attn=` still
works.

**T-state**: One clock cycle. Every Z80 instruction takes a known number of them, and this
program uses them to measure time. At 2 MHz, a T-state is 500 nanoseconds.

**TDRE**: Transmit Data Register Empty. The bit in the status register of a UART that means "the
board has room for another character". A program that prints reads this bit until it is set. A
serial port connected to `null` has it set all the time. For this reason, a program that writes
to nothing works.

**track**: One ring of sectors on a disk. The head steps in and out to reach a track, and does
not move again to reach the sectors on it. For this reason, a BIOS with a buffer keeps a whole
track at a time: after one seek, it can read or write every sector of the track.

**UART**: Universal Asynchronous Receiver/Transmitter. The chip that changes a byte into a
series of bits on a wire, and back again. The Interfacer 1 has two of them. The ACIA is a UART.

**unit**: One channel of a board that moves characters, like the connector on the back of the
board. An Interfacer 1 has two units, `a` and `b`, and you connect each one separately. `CONNECT if1:b`
names the board and then the unit.

**VI0–VI7**: The eight vectored interrupt lines of the S-100 bus. A board that pulls a VI line
does not interrupt the processor by itself. An interrupt controller watches the lines, decides
which board wins when two pull at the same time, and gives the processor the vector. On the
machines here, the System Support 1 is that controller: two 8259A chips that the guest programs.
`SHOW BUS IRQ` shows the lines.

**warm boot**: CP/M loads its own top layer (the CCP) again from the disk, because the program
that had finished could write over it. `Ctrl-C` at the `A>` prompt does a warm boot on purpose.
It is not a reset, and the machine does not stop.
