<!-- GENERATED FROM THE PROGRAM ITSELF. Do not edit by hand.
     Every default, range and description below is printed from the same tables the
     monitor resolves against, so it cannot disagree with the program you are running. -->

# Boards and their parameters

Every key below is a key you may write in a machine file, and the *same* key you
may `SET` at the monitor prompt. That is not a coincidence and it is not a
convention: a board's properties **are** its TOML schema, so there is nothing here
that could disagree with the program.

Numbers follow the one rule: **on the wire → hex, never on the wire → decimal.**
A port is hex; a baud rate and a drive count are decimal. The defaults below are
printed in each property's own base.

The catalogue is **grouped by function** — CPU, memory, disk, serial, and so on —
and within a group the boards are in **alphabetical order**.

**CPU**

| Type | What it is |
|---|---|
| [`8085`](#8085) | Generic 8085 CPU board |
| [`cpu286`](#cpu286) | CompuPro CPU 286: an 80286 on a 16-bit bus |
| [`cpu8588`](#cpu8588) | CompuPro CPU 8085/88: 8085 + 8088, one on line |
| [`cpu8687`](#cpu8687) | CompuPro CPU 86/87: an 8086 on a 16-bit bus |
| [`pascal100`](#pascal100) | Digicomp Pascal-100: Z80 + WD MicroEngine, one active |
| [`spuz`](#spuz) | CompuPro SPUZ: slave processor, own Z80H |
| [`z80`](#z80) | Generic Z80 CPU board |

**Memory**

| Type | What it is |
|---|---|
| [`memory`](#memory) | RAM/ROM board: plain, unbanked memory regions |

**Disk**

| Type | What it is |
|---|---|
| [`disk1`](#disk1) | CompuPro Disk 1: uPD765A floppy controller with DMA |
| [`disk1a`](#disk1a) | CompuPro Disk 1A: uPD765A floppy controller with DMA |
| [`disk2`](#disk2) | CompuPro Disk 2: hard disk controller (Fujitsu 2300), DMA |
| [`disk3`](#disk3) | CompuPro Disk 3: intelligent ST506 hard disk controller, DMA |
| [`jadedd`](#jadedd) | Jade Double D: double-density controller, own Z80 |
| [`mdriveh`](#mdriveh) | CompuPro M-Drive/H: 512K RAM disk on two I/O ports |
| [`selch`](#selch) | CompuPro Selector Channel: the Disk 2's DMA cycles |
| [`tarbell`](#tarbell) | Tarbell #1011: single-density floppy controller |
| [`tarbelldd`](#tarbelldd) | Tarbell #2022: double-density floppy controller |

**Serial**

| Type | What it is |
|---|---|
| [`2sio`](#2sio) | MITS 88-2SIO: two 6850 serial ports |
| [`interfacer1`](#interfacer1) | CompuPro Interfacer 1: two serial channels |
| [`interfacer3`](#interfacer3) | CompuPro Interfacer 3: eight serial channels |
| [`interfacer4`](#interfacer4) | CompuPro Interfacer 4: three serial, Centronics, parallel |
| [`io4`](#io4) | SSM IO-4: two serial + two parallel ports |

**Video**

| Type | What it is |
|---|---|
| [`pcvideo`](#pcvideo) | CompuPro PC Video: 6845 color display, PC keyboard |

**Other**

| Type | What it is |
|---|---|
| [`hostbridge`](#hostbridge) | Host Bridge: file transfer to the host (not a period board) |
| [`ss1`](#ss1) | CompuPro System Support 1: interrupts, timer, clock, serial |


## CPU

### `8085`

Generic 8085 CPU board. Decodes nothing -- it drives the bus. The 88-CPU's twin, with an 8085 core (RIM/SIM + TRAP/RST 5.5/6.5/7.5)

**Units:** `8085` (cpu)

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `clock_hz` | int | `0` | `0` .. `100000000` | Crystal on the board. 0 runs flat out -- as fast as the host can. |
| `idle` | bool | `true` | `on` \| `off` | At full speed, rest while the guest reads an empty serial line, on any port. On by default. The guest cannot see the rest. |
| `achieved_hz` | int | — | — | LIVE: T-states per real second the run loop last reached -- the crystal you got, beside the one you asked for. Read-only; 0 until it has run. **(read-only — not a key you may set)** |


### `cpu286`

CompuPro CPU 286 (451B): an 80286 in real mode (unit 80286) with 8- and 16-bit IEEE 696 transfers (SIXTN*; byte-serial, even byte first, otherwise). Starts at FFFFF0. Its own ports, which decode A0-A15 (unless j2): the Memory Manager (a byte to an even port 01F8-01FE: D0-D3 = A20-A23, on the lines mm_lines says, J3-J6) and the 80287 ports 00F8-00FF (no 80287: reads float). sockets (J7): page FF from the on-board EPROM pair (rom). waits: jumper J1. No interrupt controller: INT* (pin 73) and NMI* reach the 80286. Runs no 8-bit software. IEEE 696 bus

**Units:** `80286` (cpu)

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `mm_lines` | int | `0xF` | `0x0` .. `0xF` | Jumpers J3-J6: which of A20-A23 the Memory Manager drives (bit 0 = A20 = J6 ... bit 3 = A23 = J3); the 80286 drives the others. As shipped F (all A-C): the first fetch after reset is at 0FFFF0. 0 (all B-C): at FFFFF0 |
| `j2` | bool | `false` | `on` \| `off` | J2 fitted: the 80287 ports (00F8-00FF) and the Memory Manager (01F8-01FF) go back to the S-100 bus, and the Memory Manager cannot be written. Standard off |
| `sockets` | bool | `false` | `on` \| `off` | J7 fitted: page FF (FF0000-FFFFFF) is the on-board sockets, holding `rom`. As shipped off: page FF is memory on the S-100 bus |
| `rom` | string |  | text | The EPROM pair in U37/U38, as one 16-bit image: Intel HEX, S-records, a flat binary or builtin:<name>. Offset 0 = FF0000; 16K, 32K or 64K, repeated through page FF. Empty = no chips |
| `waits` | string | `B` | text | Jumper J1, the positions fitted (additive): A 1 memory, B 1 I/O, C 1 all, D 2 memory, E 2 I/O, F 2 all, G 4 sockets, H 1 memory write + 1 I/O. Empty = none (an illegal S-100 cycle). Default B |
| `clock_hz` | int | `0` | `0` .. `100000000` | The 80286's clock: 8000000, 10000000 or 12500000 as sold. 0 runs flat out, timed as 8000000 |
| `idle` | bool | `true` | `on` \| `off` | At full speed, rest while the guest reads an empty serial line, on any port. On by default. The guest cannot see the rest. |
| `achieved_hz` | int | — | — | LIVE: T-states per real second the run loop last reached. Read-only; 0 until it has run. **(read-only — not a key you may set)** |


### `cpu8588`

CompuPro CPU 8085/88 (161F): an 8085 (unit 8085) and an 8088 (unit 8088), one on line at a time; the 8085 starts. One port (PORT, switch S3, default FD): an IN swaps the processors and reads FF, an OUT latches the Memory Manager byte -- A16-A23 for the 8085, its high nibble on A20-A23 for the 8088. The 8088 first comes on line at FFFF0; after that each processor resumes, or is reset (swap_reset_8085/8088). Power-on-jump for the 8085 (power_on_jump, poj_addr). IOW: one wait state per I/O cycle. pINT reaches the processor on line; NMI and VI0-VI2 to the 8085's TRAP/RST pins are not modeled. IEEE 696 bus

**Units:** `8085` (cpu), `8088` (cpu)

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `port` | int | `0xFD` | `0x0` .. `0xFF` | Switch S3: the Memory Manager (OUT) and processor swap (IN) port, A0-A7. CompuPro standard FD |
| `xac` | bool | `true` | `on` \| `off` | S1-2 XAC: RESET* clears the Memory Manager (power-on always does). Quick start: on |
| `iow` | bool | `true` | `on` \| `off` | S1-3 IOW: one wait state in every I/O cycle. Quick start: on |
| `swap_reset_8085` | bool | `false` | `on` \| `off` | S1-4 5RS: reset the 8085 each time it comes on line, instead of resuming. Must be off with DMA devices. Quick start: off |
| `swap_reset_8088` | bool | `false` | `on` \| `off` | S1-5 8RS: reset the 8088 (to FFFF0) each time it comes on line, instead of resuming. Must be off with DMA devices. Quick start: off |
| `jump_on_reset` | bool | `true` | `on` \| `off` | S1-6 JOR: power-on-jump after RESET* too, not only at power-on. Quick start: on |
| `power_on_jump` | bool | `false` | `on` \| `off` | S1-8 POJ: the 8085 starts with a JMP to poj_addr (C3 00 page). Off: it starts at 0000 |
| `poj_addr` | int | `0x0` | `0x0` .. `0xFF00` | Switch S2: where the 8085's power-on-jump goes, on a 256-byte page (A8-A15) |
| `reset_8088` | bool | `true` | `on` \| `off` | Jumper J7: RESET* resets the 8088 too (A-B, factory). Off (B-C): the 8088 is reset only at power-on |
| `clock_hz_8085` | int | `0` | `0` .. `100000000` | The 8085's clock (switch S4: 6000000 or 2000000; 5000000 with the parts-list crystal). 0 runs flat out, timed as 6000000 |
| `clock_hz_8088` | int | `0` | `0` .. `100000000` | The 8088's clock (X2/3: 8000000; 5000000 with the parts-list crystal). 0 runs flat out, timed as 8000000 |
| `idle` | bool | `true` | `on` \| `off` | At full speed, rest while the guest reads an empty serial line, on any port. On by default. The guest cannot see the rest. |
| `achieved_hz` | int | — | — | LIVE: T-states per real second the run loop last reached. Read-only; 0 until it has run. **(read-only — not a key you may set)** |


### `cpu8687`

CompuPro CPU 86/87 (186B): an 8086 (unit 8086) with 8- and 16-bit IEEE 696 transfers (SIXTN*; byte-serial, even byte first, otherwise). Starts at FFFF0, or at poj_addr with power_on_jump. Its own ports, which decode A0-A15: the Memory Manager (mm_port, default 00FD: D7-D4 of an OUT = A23-A20) and a ROM-less 80130 (port_80130, default FFF0; io_80130): an 8259A-like interrupt controller and three timers in hard-wired modes 2, 0, 3, clocked by the CPU clock. The 80130's outputs (jumpers J0-J7) are not fitted. INT* (pin 73) and NMI* reach the 8086. io_wait: one wait state per I/O cycle. No 8087 (J8 fitted). Runs no 8-bit software. IEEE 696 bus

**Units:** `8086` (cpu)

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `mm_port` | int | `0xFD` | `0x0` .. `0xFFFF` | Switch S4-3..10: the Memory Manager port. A15-A8 are set, A7-A0 are FD. CompuPro standard 00FD |
| `port_80130` | int | `0xFFF0` | `0x0` .. `0xFFF0` | Switches S2 and S4-1,2: the 80130's 16 ports, on a 16-port boundary (A15-A4). CompuPro standard FFF0 |
| `io_80130` | bool | `true` | `on` \| `off` | S3-2: the 80130's ports answer. CompuPro standard on, so the system can mask its interrupts |
| `io_wait` | bool | `true` | `on` \| `off` | S3-3 and S3-4: one wait state on every I/O cycle. CompuPro standard on |
| `power_on_jump` | bool | `false` | `on` \| `off` | S5-10: start at poj_addr (a jammed JMPF) instead of FFFF0. Standard off |
| `jump_on_reset` | bool | `false` | `on` \| `off` | S5-9: the power-on jump on every RESET too, not only at power-on |
| `poj_addr` | int | `0xFF000` | `0x0` .. `0xFF000` | Switch S5-1..8 (A19-A12): where the power-on jump goes, a 4K boundary in the low megabyte. All OFF = FF000 |
| `clock_hz` | int | `0` | `0` .. `100000000` | The 8086's clock: 8000000 or 10000000 as shipped, 5000000 with an 8087. 0 runs flat out, timed as 8000000 |
| `idle` | bool | `true` | `on` \| `off` | At full speed, rest while the guest reads an empty serial line, on any port. On by default. The guest cannot see the rest. |
| `achieved_hz` | int | — | — | LIVE: T-states per real second the run loop last reached. Read-only; 0 until it has run. **(read-only — not a key you may set)** |


### `pascal100`

Digicomp Research Pascal-100: a Z80 (unit z80) and the Western Digital MicroEngine (unit me, UCSD Pascal III.0 P-code), one active at a time. IEEE 696 bus only. System Control Port at PORT (switch U225, default D0) and the Map Output Port at PORT+1, both write-only. Standard Map: 16 entries of 8K, 128K physical, per processor on or off. Z80 and MicroEngine I/O put the port on both address halves; MicroEngine words FFpp/FEpp are I/O port pp. pINT and NMI go to the Z80; NMI, and pINT with auto-transfer on, give the Z80 control. Jump-on-reset (J202, reset_addr)

**Units:** `z80` (cpu), `me` (cpu)

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `port` | int | `0xD0` | `0x0` .. `0xFE` | System Control Port (switch U225; even). The Map Output Port is the next port. Factory setting D0 |
| `jump_on_reset` | bool | `true` | `on` \| `off` | Jumper J202: after a reset the Z80 jumps to reset_addr. Factory setting on |
| `reset_addr` | int | `0x0` | `0x0` .. `0xF000` | Switch U242: where the Z80 jumps after a reset, on a 4K boundary. Factory setting 0000 |
| `clock_hz` | int | `0` | `0` .. `100000000` | The clock of both processors (header U102: 2 or 2.5 MHz in the manual, 3 MHz measured on a real box). 0 runs flat out -- as fast as the host can. |
| `idle` | bool | `true` | `on` \| `off` | At full speed, rest while the guest reads an empty serial line, on any port. On by default. The guest cannot see the rest. |
| `achieved_hz` | int | — | — | LIVE: T-states per real second the run loop last reached. Read-only; 0 until it has run. **(read-only — not a key you may set)** |


### `spuz`

CompuPro (Viasyn) SPUZ (board 257E): a slave processor with its own 8 MHz Z80H, 64K or 256K of RAM (memory) and a 2681 DUART with two RS-232 channels (units a and b). Not a bus master. To the host it is a 64K window (window = A23-A16, default 0E: 0E0000h) and ports FC30-FC41, decoded on A0-A15: FC30-FC3F the 2681, FC40 SLAVESEL (every board), FC41 PTLATCH (D3 ZRST*, D1-D0 host bank). Up to 16 boards share both; SLAVESEL against `select` (0-15) picks the one that answers. 2681 OP6 pulls a VI line (interrupt, default vi0) from reset until software sets OPR bit 6. An access while the Z80 is in reset hangs a real bus: here it floats and the run stops (bus-hang). waits: jumper J1

**Units:** `z80` (cpu), `a` (serial, CONNECT), `b` (serial, CONNECT)

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `select` | int | `0` | `0` .. `15` | The board select, SW1 positions 5-8: 0-15 (default 0). Each SPUZ in a machine needs its own; SLAVESEL (FC40h) D3-D0 picks the board that answers |
| `window` | int | `0xE` | `0x0` .. `0xFF` | Address bits A23-A16 of the 64K window, SW2: 00-FF (default 0E, the CompuPro standard: 0E0000h-0EFFFFh) |
| `memory` | enum | `256k` | `256k` \| `64k` | 256k (eight 41256s, four 64K banks; default) or 64k (eight 4164s, one bank) |
| `waits` | int | `2` | `1` .. `3` | J1: the S-100 wait states on each access to the window or the 2681: 1 (A), 2 (B, as shipped) or 3 (C) |
| `m1_wait` | bool | `true` | `on` \| `off` | SW1 position 4: one wait state in each Z80 M1 cycle (on, as shipped; needed at 6 MHz and above) |
| `history` | bool | `false` | `on` \| `off` | Record the board Z80's instructions for HISTORY CPU <id>:z80 (off by default: it costs time on every Z80 instruction) |
| `interrupt` | enum | `vi0` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J2: the VI line that 2681 OP6 pulls through Q3 (vi0, as shipped) *(interrupt strap)* |

#### Unit `a` — `[board.unit.a]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `connect` | string | `null` | text | The endpoint on the other end of this channel's line (CONNECT sets this) |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "CompuPro SPUZ a" |

#### Unit `b` — `[board.unit.b]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `connect` | string | `null` | text | The endpoint on the other end of this channel's line (CONNECT sets this) |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "CompuPro SPUZ b" |


### `z80`

Generic Z80 CPU board. Decodes nothing -- it drives the bus. The 88-CPU's twin, with a Z80 core

**Units:** `z80` (cpu)

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `clock_hz` | int | `0` | `0` .. `100000000` | Crystal on the board. 0 runs flat out -- as fast as the host can. |
| `idle` | bool | `true` | `on` \| `off` | At full speed, rest while the guest reads an empty serial line, on any port. On by default. The guest cannot see the rest. |
| `achieved_hz` | int | — | — | LIVE: T-states per real second the run loop last reached -- the crystal you got, beside the one you asked for. Read-only; 0 until it has run. **(read-only — not a key you may set)** |


## Memory

### `memory`

RAM/ROM board: a list of regions and PHANTOM* -- plain, unbanked memory

#### `[[board.region]]` — a list you may add

| Key | Kind | Legal | Meaning |
|---|---|---|---|
| `type` | enum | `ram` \| `rom` | RAM, or ROM (which needs a `mount`, unless you want an empty socket) |
| `at` | int | `0x0` .. `0xFFFFFF` | Where it starts. An address: 0000, F800; with ext = true a 24-bit one: 010000 |
| `size` | int | `1` .. `16777216` | How much. Decimal, and it takes a suffix: 48K, 1024, 2M. More than 64K needs ext = true |
| `mount` | string | text | The ROM image. A file (relative to THIS FILE), or builtin:<name> |
| `relocate` | bool | `on` \| `off` | Move a HEX/S-record image to `at` instead of its own record address |

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `honors_phantom` | enum | `all` | `none` \| `read` \| `all` | A JUMPER. Another board pulls PHANTOM* -- do I switch off? none \| read \| all |
| `phantom` | enum | `all` | `none` \| `read` \| `all` | What I ASSERT over my rom regions: none \| read \| all |
| `fill` | enum | `random` | `zero` \| `random` | RAM contents at power-on: zero \| random (real RAM is not zeroed) |
| `seed` | int | `1` | any | Seed for fill=random. The same seed fills RAM the same way at every POWER, so a run is repeatable; change it for a different junk pattern |
| `ext` | bool | `false` | `on` \| `off` | IEEE 696 extended addressing: true = decode A16-A23, region `at` is a 24-bit address and a region may be larger than 64K; false = A0-A15 only (the board answers in every 64K segment and holds at most 64K) |
| `sixteen_bit` | bool | `false` | `on` \| `off` | IEEE 696: answer SIXTN* and move a word (even + odd byte) in one cycle |
| `pages` | string | — | — | the composite page map -- which pages this board answers for. Derived from the regions you declared **(read-only — not a key you may set)** |


## Disk

### `disk1`

CompuPro Disk 1 (1981), the Disk 1A's predecessor: an NEC uPD765A floppy controller for 8" drives, single and double density, up to 4 drives. Four ports at BASE+0..3 (default C0): 765 status, 765 data, board status / DMA address, and a bit-banged serial line on D7. All data moves by DMA. Carries a 2716 boot EPROM whose selected 256-byte routine answers at 0000 over PHANTOM* until the guest switches it off; any reset turns it back on

**Units:** `drive0` (disk, MOUNT), `drive1` (disk, MOUNT), `drive2` (disk, MOUNT), `drive3` (disk, MOUNT), `serial` (serial, CONNECT)

#### `[[board.drive]]` — a list you may add

| Key | Kind | Legal | Meaning |
|---|---|---|---|
| `unit` | int | `0` .. `3` | Which drive (0..3) |
| `mount` | string | text | The disk image to put in it. Relative to THIS FILE. |
| `readonly` | bool | `on` \| `off` | Write-protect the disk. The 765 tells the guest (ST1 NW, ST3 WP); CP/M 2.2 reports it as Bad Sector *(also `writeprotect`)* |
| `media` | enum | `8ss` \| `8ss-sd` \| `8ss-dd256` \| `8ss-dd512` \| `8ss-dd1024` \| `8ds` \| `8ds-sd` \| `8ds-dd256` \| `8ds-dd512` \| `8ds-dd1024` | The diskette: its size and sides, such as 8ds, or a whole format, such as 8ds-dd1024. A blank disk needs it. Without it, the size of the image decides |
| `create` | bool | `on` \| `off` | Make an empty disk file if it is not there, then mount it as a blank disk. A blank needs `media` |

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `port` | int | `0xC0` | `0x0` .. `0xFC` | Base address (S2-3..8). The board decodes four ports: BASE+0 .. BASE+3 (default C0) |
| `rom` | string | `builtin:DISK1` | text | The boot EPROM: builtin:DISK1 (default), or a 2716 image file (Intel HEX, S-records or binary) |
| `routine` | int | `4` | `0` .. `7` | Boot routine (S2-1..2 and J17): the 256-byte page of the EPROM at 0000. J17 at A adds 4. In builtin:DISK1, 4-7 are 8080/Z80 code (default 4) |
| `boot` | bool | `true` | `on` \| `off` | Boot EPROM enable (S1-4). On (default, S1-4 OFF): after a reset the ROM answers every memory read (A0-A7 pick the byte) over PHANTOM*, until OUT BASE+3 with D0 = 0. Off: a plain disk controller |
| `dma_priority` | int | `15` | `0` .. `15` | DMA priority (S1-5..8): the TMA bus-arbitration priority, 0-15; the higher one gets the bus first. All four switches OFF = 15 (default, CompuPro's setting). Two bus masters must not share a priority |
| `port3` | enum | `serial` | `serial` \| `motor` | S1-3: what the port 3 D7 latch drives. serial (default): the RS-232 output line. motor: the MOTOR ON line of the minifloppy cable, D7 = 0 on |
| `interrupt` | enum | `none` | `none` \| `int` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | Where the 765's INT goes (J0-J7 = vi0-vi7, J8 = int; default none, polled) *(interrupt strap)* |

#### Unit `serial` — `[board.unit.serial]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `baud` | int | `9600` | `50` .. `115200` | Line rate: the far end's setting, and what the guest's delay loop must match. 8 data bits, no parity, 1 stop bit |
| `connect` | string | `null` | text | The endpoint on the other end of the line (CONNECT sets this) |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "CompuPro Disk 1 serial" |


### `disk1a`

CompuPro Disk 1A: an NEC uPD765A floppy controller for 8" and 5.25" drives, single and double density, up to 4 drives. Four ports at BASE+0..3 (default C0): 765 status / drive select, 765 data, board status / DMA address, motor control. All data moves by DMA. Carries a boot EPROM whose selected 512-byte routine answers at 0000 over PHANTOM* until the guest switches it off; any reset turns it back on

**Units:** `drive0` (disk, MOUNT), `drive1` (disk, MOUNT), `drive2` (disk, MOUNT), `drive3` (disk, MOUNT)

#### `[[board.drive]]` — a list you may add

| Key | Kind | Legal | Meaning |
|---|---|---|---|
| `unit` | int | `0` .. `3` | Which drive (0..3) |
| `mount` | string | text | The disk image to put in it. Relative to THIS FILE. |
| `readonly` | bool | `on` \| `off` | Write-protect the disk. The 765 tells the guest (ST1 NW, ST3 WP); CP/M 2.2 reports it as Bad Sector *(also `writeprotect`)* |
| `media` | enum | `8ss` \| `8ss-sd` \| `8ss-dd256` \| `8ss-dd512` \| `8ss-dd1024` \| `8ds` \| `8ds-sd` \| `8ds-dd256` \| `8ds-dd512` \| `8ds-dd1024` \| `5ss` \| `5ss-sd` \| `5ss-dd256` \| `5ss-dd512` \| `5ss-dd1024` \| `5ds` \| `5ds-sd` \| `5ds-dd256` \| `5ds-dd512` \| `5ds-dd1024` | The diskette: its size and sides, such as 8ds, or a whole format, such as 8ds-dd1024. A blank disk needs it. Without it, the size of the image decides |
| `create` | bool | `on` \| `off` | Make an empty disk file if it is not there, then mount it as a blank disk. A blank needs `media` |

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `port` | int | `0xC0` | `0x0` .. `0xFC` | Base address (S3-2..7). The board decodes four ports: BASE+0 .. BASE+3 (default C0) |
| `rom` | string | `builtin:DISK1A` | text | The boot EPROM: builtin:DISK1A (default), or a 2764 image file (Intel HEX, S-records or binary) |
| `routine` | int | `0` | `0` .. `15` | Boot routine (S1-2..5): the 512-byte page of the EPROM. In builtin:DISK1A, 0-3 are 8080/8085/Z80 code and 4-7 are 8086/286 code |
| `boot` | bool | `true` | `on` \| `off` | Boot EPROM enable (S3-8). On (default): after a reset the ROM answers every memory read (A0-A8 pick the byte) over PHANTOM*, until OUT BASE+3 with D0 = 0. Off: a plain disk controller |
| `dma_priority` | int | `15` | `0` .. `15` | DMA priority (S2-4..7): the TMA bus-arbitration priority, 0-15; the higher one gets the bus first. All four switches OFF = 15 (default, CompuPro's setting). Two bus masters must not share a priority |
| `sense_switch` | bool | `true` | `on` \| `off` | S3-1, read in status bit 2. On (default, reads 0): the CompuPro ROM tells the OS to use a System Support 1 console. Off: an Interfacer 3/4 |
| `mini_motor_control` | bool | `true` | `on` \| `off` | Jumper J11, in the 5.25" MTR ON line. On (default, J11 installed): bit 7 of the Motor Control Register runs the 5.25" motors, and they stop about 15 seconds after the last access. Off: the 5.25" motors run all the time. The 8" motors always follow bits 4-7 |
| `interrupt` | enum | `vi4` | `none` \| `int` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | Where the 765's INT goes (J10; CompuPro used vi4) *(interrupt strap)* |


### `disk2`

CompuPro Disk 2 (board 177): a hard disk controller for up to four Fujitsu M2301B/M2302B drives (m10, m20: 244 cylinders, 4 or 8 heads, 11 sectors of 1024 bytes). Two ports at BASE+0..1 (default C8): CTL/STATUS, and the DRIVE/CYLINDER/HEAD/SECTOR registers (an IN steps the selected drive). Commands NULL, READ DATA, WRITE DATA, WRITE HEADER, READ HEADER. All data moves by DMA through a Selector Channel (`selch`) programmed for its dma_priority (default 10). ATTN to a VI line or pINT when a command ends

**Units:** `drive0` (disk, MOUNT), `drive1` (disk, MOUNT), `drive2` (disk, MOUNT), `drive3` (disk, MOUNT)

#### `[[board.drive]]` — a list you may add

| Key | Kind | Legal | Meaning |
|---|---|---|---|
| `unit` | int | `0` .. `3` | Which drive (0..3): the J1-J4 data cable and DRIVE register bit 4+n |
| `mount` | string | text | The disk image to put in it. Relative to THIS FILE. |
| `readonly` | bool | `on` \| `off` | Write-protect the disk. A write gives WRITE FAULT *(also `writeprotect`)* |
| `type` | enum | `m20` \| `m10` | The drive: m20 (Fujitsu M2302B, 8 heads, default) or m10 (M2301B, 4 heads); 244 cylinders |
| `sectors` | int | `1` .. `256` | Sectors per track, set by the drive's own sector switches. Default 11 (1090-byte slots, the setting for 1024-byte sectors) |
| `create` | bool | `on` \| `off` | Make the disk file if it is not there: a new disk, filled with E5 |

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `port` | int | `0xC8` | `0x0` .. `0xFE` | Base address (SW2-1..7 = A7..A1). Two ports: BASE = CTL / STATUS, BASE+1 = data registers / STEP (default C8, CompuPro's) |
| `dma_priority` | int | `10` | `0` .. `15` | DMA priority (SW1-2..5): the TMA bus-arbitration priority, 0-15; the higher one gets the bus first. Default 10, CompuPro's. The Selector Channel's MODE byte must name it. Two bus masters must not share a priority |
| `sector_size` | enum | `1024` | `128` \| `256` \| `512` \| `1024` \| `2048` | Sector size in bytes (SW1-6..10, one switch on). Default 1024, CompuPro's. The disk images are laid out by it, so change it with no disk mounted |
| `interrupt` | enum | `none` | `none` \| `int` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | Where ATTN (command done) goes: J7-J14 = vi0-vi7, J15 = int, or none (default). CompuPro's MP/M 8-16 uses vi1 (J8) *(interrupt strap)* |


### `disk3`

CompuPro Disk 3 (board 194): an ST506 Winchester controller with its own 8085, up to four drives (default shape Quantum Q540: 512 cylinders, 8 heads, 9 sectors of 1024 bytes). One write-only attention port pair (default 90): 00 = attention, the J10 bit = reset. The host builds 16-byte IOPBs in memory, linked from 0:50h; the board fetches them, moves the data and writes STATUS back by its own DMA at dma_priority (default 9). Commands NOOP, VERSION, GLOBAL, SPECIFY, SET-MAP, HOME, SEEK, READ-HEADER, R/W, FORMAT, FORMAT-BAD. Completion interrupt to a VI line or pINT

**Units:** `drive0` (disk, MOUNT), `drive1` (disk, MOUNT), `drive2` (disk, MOUNT), `drive3` (disk, MOUNT)

#### `[[board.drive]]` — a list you may add

| Key | Kind | Legal | Meaning |
|---|---|---|---|
| `unit` | int | `0` .. `3` | Which drive (0..3): the J1-J4 cable, the IOPB DRIVE byte |
| `mount` | string | text | The disk image to put in it. Relative to THIS FILE. |
| `readonly` | bool | `on` \| `off` | Write-protect the disk. A write gives STATUS 05 (write fault) *(also `writeprotect`)* |
| `cylinders` | int | `1` .. `4096` | Cylinders on the drive. Default 512 (Quantum Q540) |
| `heads` | int | `1` .. `16` | Heads on the drive. Default 8 (Quantum Q540) |
| `sectors` | int | `1` .. `64` | Sectors per track as formatted. Default 9 (CompuPro: 9 x 1024 on ST506) |
| `sector_size` | int | `128` .. `2048` | Bytes per sector as formatted. Default 1024 |
| `create` | bool | `on` \| `off` | Make the disk file if it is not there: a new disk, filled with E5 |

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `port` | int | `0x90` | `0x0` .. `0xFE` | Attention port (S1-1..7 = A7..A1): a write-only pair, BASE and BASE+1. Default 90, CompuPro's |
| `dma_priority` | int | `9` | `0` .. `15` | DMA priority (J11 P0-P3): the TMA bus-arbitration priority, 0-15; the higher one gets the bus first. Default 9, the manual's. Two bus masters must not share a priority |
| `reset_bit` | int | `0` | `0` .. `7` | The data bit that resets this board (J10), 0-7. Default 0: CompuPro's BIOS writes 01 to reset the first board, the manual FF for all |
| `interrupt` | enum | `none` | `none` \| `int` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | Where the completion interrupt goes (J9): vi0-vi7, int, or none (default). The manual says vi1 in a CompuPro multi-user system *(interrupt strap)* |


### `jadedd`

Jade Double D Rev C: an intelligent double-density floppy controller with its own Z80A, 2 K RAM and WD FD1791. The host loads a program (Jade's DCM2) through a 1 K window at E000-FC00 and talks to it through one port (40-43, default 43). Up to 4 drives

**Units:** `drive0` (disk, MOUNT), `drive1` (disk, MOUNT), `drive2` (disk, MOUNT), `drive3` (disk, MOUNT)

#### `[[board.drive]]` — a list you may add

| Key | Kind | Legal | Meaning |
|---|---|---|---|
| `unit` | int | `0` .. `3` | Which drive (0..3) |
| `mount` | string | text | The disk image to put in it. Relative to THIS FILE. |
| `readonly` | bool | `on` \| `off` | Write-protect the disk. The WD17xx tells the guest (status S6 WRITE PROTECT) and refuses the write *(also `writeprotect`)* |

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `port` | int | `0x43` | `0x40` .. `0x43` | The host I/O port, set by switches P0 and P1: 40, 41, 42 or 43 (default 43, the port the Jade software uses) |
| `window` | int | `0xE000` | `0xE000` .. `0xFC00` | Where the host sees the 1 K memory window, set by switches M10-M12: a 1 K boundary from E000 to FC00 (default E000) |
| `address_mode` | enum | `std` | `std` \| `ext` | The ADDRESS MODE jumper. std (M-S, as shipped): a 16-bit bus, the window is in every 64K segment. ext (M-E, IC 4B fitted): a 24-bit bus, the window is in segment FF only |
| `phantom` | bool | `false` | `on` \| `off` | The PHAN* jumper. On: the board pulls PHANTOM* while the host reads the window, so the window can overlap RAM that honors PHANTOM* |
| `interrupt` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | The VI line the board pulls when its Z80 has halted (none by default) *(interrupt strap)* |
| `size` | enum | `8` | `8` \| `5` | The CLOCK SELECT jumper. 8: 8-inch drives, Z80 at 4 MHz, FD1791 at 2 MHz. 5: 5.25-inch drives, Z80 at 2 MHz, FD1791 at 1 MHz |


### `mdriveh`

CompuPro M-Drive/H (board 197B): 512K of dynamic RAM used as a disk drive, on two I/O ports (default C6H data, C7H address; S1-1..7 = A7-A1). Each OUT to the address port shifts a 22-bit counter up 8 bits and loads the byte; each data IN/OUT moves one byte and counts up. Up to eight boards share the same two ports: MA21-MA19 against `cascade` (S1-8..10) pick the board that stores and drives, and a range with no board reads FF. RAM contents at power-on per `fill`. CompuPro CP/M 2.2, CP/M-86 and MP/M 8-16 size the boards and use them as drive M:

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `port` | int | `0xC6` | `0x0` .. `0xFE` | Switch S1-1..7 (A7-A1): the data port; the address port is the next one. CompuPro software uses C6H |
| `cascade` | int | `0` | `0` .. `7` | Switch S1-8..10: which 512K of the 4M address range this board holds, 0-7. The 1st board is 0; every board in the system needs its own |
| `fill` | enum | `random` | `zero` \| `random` | RAM contents at power-on: zero \| random (real DRAM is not zeroed) |
| `seed` | int | `1` | any | Seed for fill=random. The same seed fills the RAM the same way at every POWER, so a run is repeatable |


### `selch`

CompuPro Selector Channel (board 172): the 24-bit address, status and strobes of a temporary master, shared by the controllers that use it (the Disk 2). One port (default F0): an IN disables it, then four OUTs load A23-A16, A15-A8, A7-A0 and the MODE byte (write/read, memory/I/O, up/down, wait, and the complement of the controller's DMA priority). The boot EPROM is not modeled

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `port` | int | `0xF0` | `0x0` .. `0xFF` | The board's one I/O port (SW1-3..10 = A7..A0; default F0, CompuPro's). OUT shifts in A23-A16, A15-A8, A7-A0, MODE; IN disables the board |


### `tarbell`

Tarbell #1011: single-density WD FD1771 floppy, up to 4 drives. Eight ports at BASE+0..7 (default F8). Carries a 32-byte boot PROM that shadows 0000 over PHANTOM* -- boots CP/M automatically at reset (bootstrap=on)

**Units:** `drive0` (disk, MOUNT), `drive1` (disk, MOUNT), `drive2` (disk, MOUNT), `drive3` (disk, MOUNT)

#### `[[board.drive]]` — a list you may add

| Key | Kind | Legal | Meaning |
|---|---|---|---|
| `unit` | int | `0` .. `3` | Which drive (0..3) |
| `mount` | string | text | The disk image to put in it. Relative to THIS FILE. |
| `readonly` | bool | `on` \| `off` | Write-protect the disk. The WD17xx tells the guest (status S6 WRITE PROTECT) and refuses the write *(also `writeprotect`)* |

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `bootstrap` | bool | `true` | `on` \| `off` | The boot-PROM enable DIP. On (default): the 32-byte PROM shadows 0000 over PHANTOM* at reset and boots the disk. Off: a plain disk controller, no PROM |
| `port` | int | `0xF8` | `0x0` .. `0xF8` | Base address. The board decodes eight ports: BASE+0 .. BASE+7 (default F8) |
| `drives` | int | `4` | `1` .. `4` | Drives on the controller (binary select 0-3) |
| `interrupt` | enum | `none` | `none` \| `int` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | Where the card's interrupt is soldered *(interrupt strap)* |
| `timing` | enum | `full` | `full` \| `real` | Disk timing. full: a disk access completes at once. real: the wait port holds READY until the next byte or the end of the command, so seek, head settle and byte times take emulated time, as on the board |


### `tarbelldd`

Tarbell #2022: double-density WD FD1791 floppy (mixed-density media, SD track 0), up to 4 drives. The single-density card's twin with a bitmap OUT-FC latch and a port-FD DMA/ext-addr register. Same 32-byte boot PROM

**Units:** `drive0` (disk, MOUNT), `drive1` (disk, MOUNT), `drive2` (disk, MOUNT), `drive3` (disk, MOUNT)

#### `[[board.drive]]` — a list you may add

| Key | Kind | Legal | Meaning |
|---|---|---|---|
| `unit` | int | `0` .. `3` | Which drive (0..3) |
| `mount` | string | text | The disk image to put in it. Relative to THIS FILE. |
| `readonly` | bool | `on` \| `off` | Write-protect the disk. The WD17xx tells the guest (status S6 WRITE PROTECT) and refuses the write *(also `writeprotect`)* |

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `bootstrap` | bool | `true` | `on` \| `off` | The boot-PROM enable DIP. On (default): the 32-byte PROM shadows 0000 over PHANTOM* at reset and boots the disk. Off: a plain disk controller, no PROM |
| `port` | int | `0xF8` | `0x0` .. `0xF8` | Base address. The board decodes eight ports: BASE+0 .. BASE+7 (default F8) |
| `drives` | int | `4` | `1` .. `4` | Drives on the controller (binary select 0-3) |
| `interrupt` | enum | `none` | `none` \| `int` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | Where the card's interrupt is soldered *(interrupt strap)* |
| `timing` | enum | `full` | `full` \| `real` | Disk timing. full: a disk access completes at once. real: the wait port holds READY until the next byte or the end of the command, so seek, head settle and byte times take emulated time, as on the board |
| `dmaport` | int | `0xE0` | `0x0` .. `0xF0` | Base of the on-card 8257 DMA controller's 16-port register block (default E0). The DMA-mode CBIOS programs ADR/WCT here; the SD2DD strap is E0 |


## Serial

### `2sio`

MITS 88-2SIO: two 6850 ACIAs, units 'a' and 'b'. Four ports at BASE+0..3

**Units:** `a` (serial, CONNECT), `b` (serial, CONNECT)

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `port` | int | `0x10` | `0x0` .. `0xFC` | Base address. The board decodes four ports: BASE+0 .. BASE+3 |

#### Unit `a` — `[board.unit.a]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `baud` | int | `9600` | `50` .. `76800` | Line rate. A JUMPER on the real card -- software cannot change it, and there is no free-running setting: the rate paces the line |
| `interrupt` | enum | `none` | `none` \| `int` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | Where this channel's IRQ is jumpered: none \| int \| vi0..vi7 *(interrupt strap)* |
| `dcd` | enum | `ground` | `ground` \| `wired` | /DCD pin: grounded on the card, or wired to the connector |
| `cts` | enum | `ground` | `ground` \| `wired` | /CTS pin: grounded on the card, or wired -- and then it gates the transmitter |
| `lines` | string | — | — | Live pin state (read-only). CAPITALS = asserted. in: DCD CTS, out: RTS BRK **(read-only — not a key you may set)** |
| `connect` | string | `null` | text | The endpoint on the other end of the line (CONNECT sets this) |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "MITS 88-2SIO a" |

#### Unit `b` — `[board.unit.b]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `baud` | int | `9600` | `50` .. `76800` | Line rate. A JUMPER on the real card -- software cannot change it, and there is no free-running setting: the rate paces the line |
| `interrupt` | enum | `none` | `none` \| `int` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | Where this channel's IRQ is jumpered: none \| int \| vi0..vi7 *(interrupt strap)* |
| `dcd` | enum | `ground` | `ground` \| `wired` | /DCD pin: grounded on the card, or wired to the connector |
| `cts` | enum | `ground` | `ground` \| `wired` | /CTS pin: grounded on the card, or wired -- and then it gates the transmitter |
| `lines` | string | — | — | Live pin state (read-only). CAPITALS = asserted. in: DCD CTS, out: RTS BRK **(read-only — not a key you may set)** |
| `connect` | string | `null` | text | The endpoint on the other end of the line (CONNECT sets this) |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "MITS 88-2SIO b" |


### `interfacer1`

CompuPro Interfacer 1 (Godbout 133H): two serial channels 'a' and 'b', each a real 1602-family UART at its own 2-port block (switch S2/S3: port, enabled; default A at 00-01, B at 02-03), data at the even address and status at the odd one unless J14 swaps them (swap_ports). An OUT to the status address writes a control latch: each 1 bit inverts that line from its power-up strap and each 0 returns it, for the word format (data_bits 7/8, parity_sense, parity, stop_bits), the RS-232 outputs ca (RTS) and cd (DTR), and the two interrupt enables; reset clears the latch. The status byte is TBMT D0, DAV D1, OPT D2 (opt = cf or eoc), PE D3, OR D4, FE D5, CC D6, CB D7; the RS-232 inputs read 0 when ON. Per channel, RxINT (DAV and the RX enable) and TxINT (TBMT and the TX enable) are jumpered on J11 to vi0..vi7 or none (rx_int/tx_int); J11 has no pin-73 position. Baud from switch S1, 50-19200. CONNECT each channel to a file, socket, serial port, in:/out:

**Units:** `a` (serial, CONNECT), `b` (serial, CONNECT)

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `swap_ports` | bool | `false` | `on` \| `off` | J14: move the trace from B-C to A-B, so both channels have status at the switch address and data at +1. Off (factory) = data first, status second |

#### Unit `a` — `[board.unit.a]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `port` | int | `0x0` | `0x0` .. `0xFE` | Switch S2 (Channel A) / S3 (Channel B), positions 1-7: the channel's 2-port block, A7-A1, so an even address |
| `enabled` | bool | `true` | `on` \| `off` | Switch S2/S3 position 8: off = ON = the channel is removed from the I/O space |
| `baud` | enum | `9600` | `50` \| `75` \| `110` \| `134.5` \| `150` \| `300` \| `600` \| `1200` \| `1800` \| `2000` \| `2400` \| `3600` \| `4800` \| `7200` \| `9600` \| `19200` | Switch S1 (positions 1-4 for A, 5-8 for B): the BR1941 rate |
| `data_bits` | enum | `8` | `7` \| `8` | Power-up NBI (J7/J8 1-16 = 7, 2-15 = 8). Control bit D7 flips it |
| `parity_sense` | enum | `odd` | `odd` \| `even` | Power-up EPS (J7/J8 3-14 = odd, 4-13 = even). Control bit D6 flips it |
| `parity` | enum | `off` | `on` \| `off` | Power-up NP (J7/J8 5-12 = on, 6-11 = off). Control bit D5 flips it |
| `stop_bits` | enum | `1` | `1` \| `2` | Power-up TSB (J7/J8 7-10 = 1, 8-9 = 2). Control bit D4 flips it |
| `ca` | enum | `spacing` | `spacing` \| `marking` | Power-up CA, RS-232 RTS (J9/J10 1-16 = spacing/ON, 2-15 = marking/OFF). Control bit D3 flips it |
| `cd` | enum | `spacing` | `spacing` \| `marking` | Power-up CD, RS-232 DTR (J9/J10 3-14 = spacing/ON, 4-13 = marking/OFF). Control bit D2 flips it |
| `tx_int_enable` | enum | `off` | `off` \| `on` | Power-up TXINTE (J9/J10 5-12 = off, 6-11 = on). Control bit D1 flips it |
| `rx_int_enable` | enum | `off` | `off` \| `on` | Power-up RXINTE (J9/J10 7-10 = off, 8-9 = on). Control bit D0 flips it |
| `opt` | enum | `cf` | `cf` \| `eoc` | J12 (A) / J13 (B): what status bit D2 reads -- cf = the RS-232 CF input (B-A, reads 0 when ON), eoc = the UART's End Of Character (B-C) |
| `rs232_inputs` | enum | `endpoint` | `endpoint` \| `open` | What drives the RS-232 inputs CB (CTS, D7), CC (DSR, D6) and CF (carrier, D2 with opt = cf). endpoint (default) = the endpoint's lines: a console or socket asserts them all, as a cable that straps them ON. open = a 3-wire cable: nothing is wired to them, and they read OFF (1), as an open 1489 input does on the real board |
| `overrun` | bool | `false` | `on` \| `off` | on = the line runs free at the baud rate: a character that arrives before the last one is read replaces it, and status D4 (OR) reads 1, as on the real board. off (default) = the line waits for the read, and no character is lost |
| `rx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J11: where RxINT (DAV AND RXINTE) is jumpered: none \| vi0..vi7 *(interrupt strap)* |
| `tx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J11: where TxINT (TBMT AND TXINTE) is jumpered: none \| vi0..vi7. A LEVEL, held while the transmitter is idle *(interrupt strap)* |
| `control` | int | — | — | What the guest last wrote to the control port (the 74LS175 latches) **(read-only — not a key you may set)** |
| `connect` | string | `null` | text | The endpoint on this channel's line (CONNECT sets this): a file, socket, serial port, in:/out: file, null, loopback |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "CompuPro Interfacer 1 a" |

#### Unit `b` — `[board.unit.b]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `port` | int | `0x2` | `0x0` .. `0xFE` | Switch S2 (Channel A) / S3 (Channel B), positions 1-7: the channel's 2-port block, A7-A1, so an even address |
| `enabled` | bool | `true` | `on` \| `off` | Switch S2/S3 position 8: off = ON = the channel is removed from the I/O space |
| `baud` | enum | `9600` | `50` \| `75` \| `110` \| `134.5` \| `150` \| `300` \| `600` \| `1200` \| `1800` \| `2000` \| `2400` \| `3600` \| `4800` \| `7200` \| `9600` \| `19200` | Switch S1 (positions 1-4 for A, 5-8 for B): the BR1941 rate |
| `data_bits` | enum | `8` | `7` \| `8` | Power-up NBI (J7/J8 1-16 = 7, 2-15 = 8). Control bit D7 flips it |
| `parity_sense` | enum | `odd` | `odd` \| `even` | Power-up EPS (J7/J8 3-14 = odd, 4-13 = even). Control bit D6 flips it |
| `parity` | enum | `off` | `on` \| `off` | Power-up NP (J7/J8 5-12 = on, 6-11 = off). Control bit D5 flips it |
| `stop_bits` | enum | `1` | `1` \| `2` | Power-up TSB (J7/J8 7-10 = 1, 8-9 = 2). Control bit D4 flips it |
| `ca` | enum | `spacing` | `spacing` \| `marking` | Power-up CA, RS-232 RTS (J9/J10 1-16 = spacing/ON, 2-15 = marking/OFF). Control bit D3 flips it |
| `cd` | enum | `spacing` | `spacing` \| `marking` | Power-up CD, RS-232 DTR (J9/J10 3-14 = spacing/ON, 4-13 = marking/OFF). Control bit D2 flips it |
| `tx_int_enable` | enum | `off` | `off` \| `on` | Power-up TXINTE (J9/J10 5-12 = off, 6-11 = on). Control bit D1 flips it |
| `rx_int_enable` | enum | `off` | `off` \| `on` | Power-up RXINTE (J9/J10 7-10 = off, 8-9 = on). Control bit D0 flips it |
| `opt` | enum | `cf` | `cf` \| `eoc` | J12 (A) / J13 (B): what status bit D2 reads -- cf = the RS-232 CF input (B-A, reads 0 when ON), eoc = the UART's End Of Character (B-C) |
| `rs232_inputs` | enum | `endpoint` | `endpoint` \| `open` | What drives the RS-232 inputs CB (CTS, D7), CC (DSR, D6) and CF (carrier, D2 with opt = cf). endpoint (default) = the endpoint's lines: a console or socket asserts them all, as a cable that straps them ON. open = a 3-wire cable: nothing is wired to them, and they read OFF (1), as an open 1489 input does on the real board |
| `overrun` | bool | `false` | `on` \| `off` | on = the line runs free at the baud rate: a character that arrives before the last one is read replaces it, and status D4 (OR) reads 1, as on the real board. off (default) = the line waits for the read, and no character is lost |
| `rx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J11: where RxINT (DAV AND RXINTE) is jumpered: none \| vi0..vi7 *(interrupt strap)* |
| `tx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J11: where TxINT (TBMT AND TXINTE) is jumpered: none \| vi0..vi7. A LEVEL, held while the transmitter is idle *(interrupt strap)* |
| `control` | int | — | — | What the guest last wrote to the control port (the 74LS175 latches) **(read-only — not a key you may set)** |
| `connect` | string | `null` | text | The endpoint on this channel's line (CONNECT sets this): a file, socket, serial port, in:/out: file, null, loopback |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "CompuPro Interfacer 1 b" |


### `interfacer3`

CompuPro Interfacer 3 (board 174): eight serial channels user0..user7, each a 2651 USART, behind one 8-port block (port, a multiple of 8, default 10H; enabled = S1-1). Relative ports 0-3 are the selected USART's data, status, mode and command; 4 and 5 read TxRDY0-7 / RxRDY0-7 and write the transmit / receive interrupt masks; 6-7 write the user select latch (D0-D2 user, D3-D4 board). Every board at the same port latches every select write and answers only while D3-D4 match its users switch (0-7, 8-15, 16-23, 24-31), so up to four boards share one block. Per user, TxINT (TxRDY and its mask bit) and RxINT (RxRDY and its mask bit) are jumpered to vi0..vi7 or none (tx_int/rx_int); reset clears the masks and the latch. waits (J17) adds 0-2 wait states to USART accesses. Unconnected RS-232 inputs read ON. CONNECT each user to a file, socket, serial port, in:/out:

**Units:** `user0` (serial, CONNECT), `user1` (serial, CONNECT), `user2` (serial, CONNECT), `user3` (serial, CONNECT), `user4` (serial, CONNECT), `user5` (serial, CONNECT), `user6` (serial, CONNECT), `user7` (serial, CONNECT)

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `port` | int | `0x10` | `0x0` .. `0xF8` | Switch S1-2..6 (A7-A3): the base of the 8-port block, a multiple of 8. CompuPro software uses 10H. S1-1 is `enabled` |
| `users` | enum | `0-7` | `0-7` \| `8-15` \| `16-23` \| `24-31` | Switches S1-7/S1-8: which group of eight users this board answers, by bits D3-D4 of the user select byte. Up to four boards share one block |
| `waits` | enum | `0` | `0` \| `1` \| `2` | J17: wait states on each USART access (relative ports 0-3). 0 = no shunt, 1 = A-C, 2 = B-C. The interrupt and user select ports never wait |
| `select` | int | — | — | The user select latch U34 (D0-D2 user, D3-D4 board), as last written **(read-only — not a key you may set)** |
| `tx_mask` | int | — | — | The transmit interrupt mask (relative port 4), as last written **(read-only — not a key you may set)** |
| `rx_mask` | int | — | — | The receive interrupt mask (relative port 5), as last written **(read-only — not a key you may set)** |

#### Unit `user0` — `[board.unit.user0]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `baud` | int | `9600` | `50` .. `19200` | Line rate. The 2651 generates it from Mode Register 2, so the guest's MR2 write overwrites this; it seeds the line before then (min 50) |
| `connect` | string | `null` | text | The endpoint on the other end of the line (CONNECT sets this) |
| `tx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J15: where TxINT (TxRDY AND the user's tx_mask bit) is jumpered: none \| vi0..vi7. CompuPro's MP/M 8-16 manual puts every TxINT on one line *(interrupt strap)* |
| `rx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J16: where RxINT (RxRDY AND the user's rx_mask bit) is jumpered: none \| vi0..vi7 *(interrupt strap)* |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "CompuPro Interfacer 3 port 0" |

#### Unit `user1` — `[board.unit.user1]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `baud` | int | `9600` | `50` .. `19200` | Line rate. The 2651 generates it from Mode Register 2, so the guest's MR2 write overwrites this; it seeds the line before then (min 50) |
| `connect` | string | `null` | text | The endpoint on the other end of the line (CONNECT sets this) |
| `tx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J15: where TxINT (TxRDY AND the user's tx_mask bit) is jumpered: none \| vi0..vi7. CompuPro's MP/M 8-16 manual puts every TxINT on one line *(interrupt strap)* |
| `rx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J16: where RxINT (RxRDY AND the user's rx_mask bit) is jumpered: none \| vi0..vi7 *(interrupt strap)* |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "CompuPro Interfacer 3 port 1" |

#### Unit `user2` — `[board.unit.user2]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `baud` | int | `9600` | `50` .. `19200` | Line rate. The 2651 generates it from Mode Register 2, so the guest's MR2 write overwrites this; it seeds the line before then (min 50) |
| `connect` | string | `null` | text | The endpoint on the other end of the line (CONNECT sets this) |
| `tx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J15: where TxINT (TxRDY AND the user's tx_mask bit) is jumpered: none \| vi0..vi7. CompuPro's MP/M 8-16 manual puts every TxINT on one line *(interrupt strap)* |
| `rx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J16: where RxINT (RxRDY AND the user's rx_mask bit) is jumpered: none \| vi0..vi7 *(interrupt strap)* |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "CompuPro Interfacer 3 port 2" |

#### Unit `user3` — `[board.unit.user3]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `baud` | int | `9600` | `50` .. `19200` | Line rate. The 2651 generates it from Mode Register 2, so the guest's MR2 write overwrites this; it seeds the line before then (min 50) |
| `connect` | string | `null` | text | The endpoint on the other end of the line (CONNECT sets this) |
| `tx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J15: where TxINT (TxRDY AND the user's tx_mask bit) is jumpered: none \| vi0..vi7. CompuPro's MP/M 8-16 manual puts every TxINT on one line *(interrupt strap)* |
| `rx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J16: where RxINT (RxRDY AND the user's rx_mask bit) is jumpered: none \| vi0..vi7 *(interrupt strap)* |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "CompuPro Interfacer 3 port 3" |

#### Unit `user4` — `[board.unit.user4]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `baud` | int | `9600` | `50` .. `19200` | Line rate. The 2651 generates it from Mode Register 2, so the guest's MR2 write overwrites this; it seeds the line before then (min 50) |
| `connect` | string | `null` | text | The endpoint on the other end of the line (CONNECT sets this) |
| `tx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J15: where TxINT (TxRDY AND the user's tx_mask bit) is jumpered: none \| vi0..vi7. CompuPro's MP/M 8-16 manual puts every TxINT on one line *(interrupt strap)* |
| `rx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J16: where RxINT (RxRDY AND the user's rx_mask bit) is jumpered: none \| vi0..vi7 *(interrupt strap)* |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "CompuPro Interfacer 3 port 4" |

#### Unit `user5` — `[board.unit.user5]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `baud` | int | `9600` | `50` .. `19200` | Line rate. The 2651 generates it from Mode Register 2, so the guest's MR2 write overwrites this; it seeds the line before then (min 50) |
| `connect` | string | `null` | text | The endpoint on the other end of the line (CONNECT sets this) |
| `tx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J15: where TxINT (TxRDY AND the user's tx_mask bit) is jumpered: none \| vi0..vi7. CompuPro's MP/M 8-16 manual puts every TxINT on one line *(interrupt strap)* |
| `rx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J16: where RxINT (RxRDY AND the user's rx_mask bit) is jumpered: none \| vi0..vi7 *(interrupt strap)* |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "CompuPro Interfacer 3 port 5" |

#### Unit `user6` — `[board.unit.user6]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `baud` | int | `9600` | `50` .. `19200` | Line rate. The 2651 generates it from Mode Register 2, so the guest's MR2 write overwrites this; it seeds the line before then (min 50) |
| `connect` | string | `null` | text | The endpoint on the other end of the line (CONNECT sets this) |
| `tx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J15: where TxINT (TxRDY AND the user's tx_mask bit) is jumpered: none \| vi0..vi7. CompuPro's MP/M 8-16 manual puts every TxINT on one line *(interrupt strap)* |
| `rx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J16: where RxINT (RxRDY AND the user's rx_mask bit) is jumpered: none \| vi0..vi7 *(interrupt strap)* |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "CompuPro Interfacer 3 port 6" |

#### Unit `user7` — `[board.unit.user7]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `baud` | int | `9600` | `50` .. `19200` | Line rate. The 2651 generates it from Mode Register 2, so the guest's MR2 write overwrites this; it seeds the line before then (min 50) |
| `connect` | string | `null` | text | The endpoint on the other end of the line (CONNECT sets this) |
| `tx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J15: where TxINT (TxRDY AND the user's tx_mask bit) is jumpered: none \| vi0..vi7. CompuPro's MP/M 8-16 manual puts every TxINT on one line *(interrupt strap)* |
| `rx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J16: where RxINT (RxRDY AND the user's rx_mask bit) is jumpered: none \| vi0..vi7 *(interrupt strap)* |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "CompuPro Interfacer 3 port 7" |


### `interfacer4`

CompuPro Interfacer 4 (board 187C): four users behind the Interfacer 3's 8-port block (port, a multiple of 8, default 10H; enabled = S2-4). users (S2-1..3) picks the four: half of a group of eight, so two boards make a group (default 4-7). Relative user 0 = the parallel ports: port 0 writes the Centronics printer (unit centronics) and reads the sense switch S1 (sense), port 1 reads Status-C and writes Control-C, port 2 is the Universal Parallel data (unit parallel), port 3 reads Status-P. Users 1-3 (units user1..user3) are 2651 USARTs. Ports 4/5 read this board's nibble of Tx/Rx ready and write its nibble of the interrupt masks; 6-7 write the user select latch, which every board at the port takes. tx_int/rx_int jumper each source to vi0..vi7. j26 swaps users 0 and 2; waits (J6-J8) adds 0-3 wait states. CONNECT centronics to out:FILE or printer:QUEUE

**Units:** `centronics` (serial, CONNECT), `parallel` (serial, CONNECT), `user1` (serial, CONNECT), `user2` (serial, CONNECT), `user3` (serial, CONNECT)

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `port` | int | `0x10` | `0x0` .. `0xF8` | Switch S2-5..9 (A7-A3): the base of the 8-port block, a multiple of 8. CompuPro software uses 10H. S2-4 is `enabled` |
| `users` | enum | `4-7` | `0-3` \| `4-7` \| `8-11` \| `12-15` \| `16-19` \| `20-23` \| `24-27` \| `28-31` | Switches S2-1/S2-2 (the group of eight, D3-D4 of the user select byte) and S2-3 (the half, D2). Two boards make a group. S3 follows S2-3, as page 10 of the manual requires. CompuPro software uses 4-7 |
| `waits` | enum | `0` | `0` \| `1` \| `2` \| `3` | J6/J7/J8: wait states on each access of ports 0-3. The interrupt and user select ports never wait. Set every board at the base the same |
| `j26` | enum | `ab-cd` | `ab-cd` \| `ac-bd` | J26: ab-cd = the parallel ports are relative user 0 (standard, and what MP/M 8-16 needs); ac-bd = they are relative user 2 and the middle serial channel is user 0. The status and mask bits do NOT move |
| `sense` | int | `0xFF` | `0x0` .. `0xFF` | Switch S1-3..10, read at relative port 0 of the parallel user: bit 7 = S1-3 ... bit 0 = S1-10. A switch ON reads 0. The manual's set-up has all OFF |
| `select` | int | — | — | The user select latch U18 (D0-D1 user, D2 half, D3-D4 group), as last written **(read-only — not a key you may set)** |
| `tx_mask` | int | — | — | This board's transmit interrupt mask (bit 0 the Centronics port, bits 1-3 users 1-3) **(read-only — not a key you may set)** |
| `rx_mask` | int | — | — | This board's receive interrupt mask (bit 0 the Universal Parallel port, bits 1-3 users 1-3) **(read-only — not a key you may set)** |
| `control` | int | — | — | Control-C (U13): D0 AFX, D1 INIT, D2 SLCTIN, D3 LED **(read-only — not a key you may set)** |

#### Unit `centronics` — `[board.unit.centronics]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `connect` | string | `null` | text | The printer on the Centronics port: out:FILE, printer:QUEUE, a socket, ... (CONNECT sets this) |
| `tx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | JS5: where TxINT of the parallel user (the printer's acknowledge AND tx_mask bit 0) is jumpered: none \| vi0..vi7 *(interrupt strap)* |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "CompuPro Interfacer 4 port 4 centronics" |

#### Unit `parallel` — `[board.unit.parallel]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `connect` | string | `null` | text | The device on the Universal Parallel port (CONNECT sets this) |
| `rx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | JS6: where RxINT of the parallel user (input latched AND rx_mask bit 0) is jumpered: none \| vi0..vi7 *(interrupt strap)* |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "CompuPro Interfacer 4 port 4 parallel" |

#### Unit `user1` — `[board.unit.user1]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `baud` | int | `9600` | `50` .. `19200` | Line rate. The 2651 generates it from Mode Register 2, so the guest's MR2 write overwrites this; it seeds the line before then (min 50) |
| `connect` | string | `null` | text | The endpoint on the other end of the line (CONNECT sets this) |
| `tx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | JS5: where TxINT (TxRDY AND the user's tx_mask bit) is jumpered: none \| vi0..vi7. CompuPro's MP/M 8-16 manual puts every TxINT on one line *(interrupt strap)* |
| `rx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | JS6: where RxINT (RxRDY AND the user's rx_mask bit) is jumpered: none \| vi0..vi7 *(interrupt strap)* |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "CompuPro Interfacer 4 port 5" |

#### Unit `user2` — `[board.unit.user2]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `baud` | int | `9600` | `50` .. `19200` | Line rate. The 2651 generates it from Mode Register 2, so the guest's MR2 write overwrites this; it seeds the line before then (min 50) |
| `connect` | string | `null` | text | The endpoint on the other end of the line (CONNECT sets this) |
| `tx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | JS5: where TxINT (TxRDY AND the user's tx_mask bit) is jumpered: none \| vi0..vi7. CompuPro's MP/M 8-16 manual puts every TxINT on one line *(interrupt strap)* |
| `rx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | JS6: where RxINT (RxRDY AND the user's rx_mask bit) is jumpered: none \| vi0..vi7 *(interrupt strap)* |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "CompuPro Interfacer 4 port 6" |

#### Unit `user3` — `[board.unit.user3]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `baud` | int | `9600` | `50` .. `19200` | Line rate. The 2651 generates it from Mode Register 2, so the guest's MR2 write overwrites this; it seeds the line before then (min 50) |
| `connect` | string | `null` | text | The endpoint on the other end of the line (CONNECT sets this) |
| `tx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | JS5: where TxINT (TxRDY AND the user's tx_mask bit) is jumpered: none \| vi0..vi7. CompuPro's MP/M 8-16 manual puts every TxINT on one line *(interrupt strap)* |
| `rx_int` | enum | `none` | `none` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | JS6: where RxINT (RxRDY AND the user's rx_mask bit) is jumpered: none \| vi0..vi7 *(interrupt strap)* |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "CompuPro Interfacer 4 port 7" |


### `io4`

SSM IO-4 (2P+2S): the real Solid State Music board -- two full-duplex serial channels AND a four-port parallel section. Serial units 'a'/'b' are real 1602-family UARTs with programmable word length (data_bits 5-8), parity and stop bits, plus the full status-word strap-up (stat_* map, invert_status, port_reversal) and named profiles (default altair-rev1). A 4-port block set by switch S3 (default 0-3): Serial A status/data at BASE+0/+1, Serial B at BASE+2/+3. Parallel units 'pa'/'pb' are 8212 latched ports (input latch + service request, output latch) on their own 2-port block set by switch S4 (par_port, default 4-5): Parallel A at PAR+0, B at PAR+1; a byte the far end sends is strobed in, a write latches out, and the §3.2.2 status/data console flag is strappable (dav_bit/dav_source/dav_active_low). If the two blocks overlap, neither section answers the shared ports. Each serial channel's receive (DAV) and transmit (TBMT) and each parallel input (service request) can raise an interrupt, strapped on header W4 to a VI line, pin 73 (int), or none (rx_int/tx_int per serial channel, int per parallel port) -- there is no software enable, the strap is the enable, and a parallel input interrupt rises even when the port is not addressed. Configure each unit under [board.unit.a] / [board.unit.pa] etc.; CONNECT each to a file, socket, serial port, in:/out:

**Units:** `a` (serial, CONNECT), `b` (serial, CONNECT), `pa` (serial, CONNECT), `pb` (serial, CONNECT)

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `port` | int | `0x0` | `0x0` .. `0xFC` | Serial base address (switch S3) -- a 4-PORT BLOCK, so a multiple of 4. Serial A at BASE+0/+1, Serial B at BASE+2/+3 |
| `par_port` | int | `0x4` | `0x0` .. `0xFE` | Parallel base address (switch S4) -- a 2-PORT BLOCK, so a multiple of 2. Parallel A (J6-in/J5-out) at BASE, Parallel B (J4-in/J3-out) at BASE+1. If it overlaps the serial block, neither section answers the shared ports |

#### Unit `a` — `[board.unit.a]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `profile` | enum | `altair-rev1` | `custom` \| `altair-rev1` \| `altair-rev0` \| `i8251` \| `proctech` \| `imsai` | Preset the status straps from a host personality: custom, altair-rev1 (the default and the SSM 8080 monitor console), altair-rev0, i8251, proctech, imsai. Sets stat_*, invert_status and port_reversal (overridable) |
| `stat_dav` | enum | `0` | `none` \| `0` \| `1` \| `2` \| `3` \| `4` \| `5` \| `6` \| `7` | Data-bus bit carrying DAV, data available (0-7 \| none) |
| `stat_tbmt` | enum | `7` | `none` \| `0` \| `1` \| `2` \| `3` \| `4` \| `5` \| `6` \| `7` | Data-bus bit carrying TBMT, transmit buffer empty (0-7 \| none) |
| `stat_teoc` | enum | `none` | `none` \| `0` \| `1` \| `2` \| `3` \| `4` \| `5` \| `6` \| `7` | Data-bus bit carrying TEOC, transmitter end of character (0-7 \| none) |
| `stat_ror` | enum | `none` | `none` \| `0` \| `1` \| `2` \| `3` \| `4` \| `5` \| `6` \| `7` | Data-bus bit carrying ROR, receiver over-run (0-7 \| none; always inactive) |
| `stat_rpe` | enum | `none` | `none` \| `0` \| `1` \| `2` \| `3` \| `4` \| `5` \| `6` \| `7` | Data-bus bit carrying RPE, receiver parity error (0-7 \| none; always inactive) |
| `stat_rfe` | enum | `none` | `none` \| `0` \| `1` \| `2` \| `3` \| `4` \| `5` \| `6` \| `7` | Data-bus bit carrying RFE, receiver framing error (0-7 \| none; always inactive) |
| `invert_status` | bool | `true` | `on` \| `off` | Invert every status bit -- a 74368 buffer (negative sense): an asserted signal reads 0, an inactive one reads 1. Off = 74367 (positive sense) |
| `port_reversal` | bool | `false` | `on` \| `off` | Reverse the channel's two ports (S1/S2-PR): off = status first, data last (MITS, Proc Tech); on = data first, status last (IMSAI) |
| `rx_int` | enum | `none` | `none` \| `int` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | Where the receive interrupt (a new character, DAV) is strapped on header W4: none \| int \| vi0..vi7. Canonical: vi1 (Serial A) / vi0 (Serial B) *(interrupt strap)* |
| `tx_int` | enum | `none` | `none` \| `int` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | Where the transmit interrupt (buffer empty, TBMT) is strapped on header W4: none \| int \| vi0..vi7. It is a LEVEL -- held while the transmitter is idle. Canonical: vi2 (Serial A) / vi3 (Serial B) *(interrupt strap)* |
| `baud` | int | `9600` | `50` .. `25000` | Line rate (header W3). RX and TX share one rate here -- a real host serial port cannot be split. Canonical IO-4 rates: 55-9600 |
| `data_bits` | int | `8` | `5` .. `8` | Data bits per character (S1/S2 NDB1+NDB2) |
| `stop_bits` | int | `1` | `1` .. `2` | Stop bits (S1/S2 NSB): 1 or 2 |
| `parity` | enum | `none` | `none` \| `odd` \| `even` | Parity (S1/S2 NPB/POE): none \| odd \| even |
| `connect` | string | `null` | text | The endpoint on this channel's line (CONNECT sets this): a file, socket, serial port, in:/out: file, null, loopback |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "SSM IO-4 a" |

#### Unit `b` — `[board.unit.b]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `profile` | enum | `altair-rev1` | `custom` \| `altair-rev1` \| `altair-rev0` \| `i8251` \| `proctech` \| `imsai` | Preset the status straps from a host personality: custom, altair-rev1 (the default and the SSM 8080 monitor console), altair-rev0, i8251, proctech, imsai. Sets stat_*, invert_status and port_reversal (overridable) |
| `stat_dav` | enum | `0` | `none` \| `0` \| `1` \| `2` \| `3` \| `4` \| `5` \| `6` \| `7` | Data-bus bit carrying DAV, data available (0-7 \| none) |
| `stat_tbmt` | enum | `7` | `none` \| `0` \| `1` \| `2` \| `3` \| `4` \| `5` \| `6` \| `7` | Data-bus bit carrying TBMT, transmit buffer empty (0-7 \| none) |
| `stat_teoc` | enum | `none` | `none` \| `0` \| `1` \| `2` \| `3` \| `4` \| `5` \| `6` \| `7` | Data-bus bit carrying TEOC, transmitter end of character (0-7 \| none) |
| `stat_ror` | enum | `none` | `none` \| `0` \| `1` \| `2` \| `3` \| `4` \| `5` \| `6` \| `7` | Data-bus bit carrying ROR, receiver over-run (0-7 \| none; always inactive) |
| `stat_rpe` | enum | `none` | `none` \| `0` \| `1` \| `2` \| `3` \| `4` \| `5` \| `6` \| `7` | Data-bus bit carrying RPE, receiver parity error (0-7 \| none; always inactive) |
| `stat_rfe` | enum | `none` | `none` \| `0` \| `1` \| `2` \| `3` \| `4` \| `5` \| `6` \| `7` | Data-bus bit carrying RFE, receiver framing error (0-7 \| none; always inactive) |
| `invert_status` | bool | `true` | `on` \| `off` | Invert every status bit -- a 74368 buffer (negative sense): an asserted signal reads 0, an inactive one reads 1. Off = 74367 (positive sense) |
| `port_reversal` | bool | `false` | `on` \| `off` | Reverse the channel's two ports (S1/S2-PR): off = status first, data last (MITS, Proc Tech); on = data first, status last (IMSAI) |
| `rx_int` | enum | `none` | `none` \| `int` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | Where the receive interrupt (a new character, DAV) is strapped on header W4: none \| int \| vi0..vi7. Canonical: vi1 (Serial A) / vi0 (Serial B) *(interrupt strap)* |
| `tx_int` | enum | `none` | `none` \| `int` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | Where the transmit interrupt (buffer empty, TBMT) is strapped on header W4: none \| int \| vi0..vi7. It is a LEVEL -- held while the transmitter is idle. Canonical: vi2 (Serial A) / vi3 (Serial B) *(interrupt strap)* |
| `baud` | int | `9600` | `50` .. `25000` | Line rate (header W3). RX and TX share one rate here -- a real host serial port cannot be split. Canonical IO-4 rates: 55-9600 |
| `data_bits` | int | `8` | `5` .. `8` | Data bits per character (S1/S2 NDB1+NDB2) |
| `stop_bits` | int | `1` | `1` .. `2` | Stop bits (S1/S2 NSB): 1 or 2 |
| `parity` | enum | `none` | `none` \| `odd` \| `even` | Parity (S1/S2 NPB/POE): none \| odd \| even |
| `connect` | string | `null` | text | The endpoint on this channel's line (CONNECT sets this): a file, socket, serial port, in:/out: file, null, loopback |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "SSM IO-4 b" |

#### Unit `pa` — `[board.unit.pa]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `connect` | string | `null` | text | The endpoint on this parallel port's line (CONNECT sets this): a file, socket, printer, in:/out: file, null. A WRITE latches out; a byte the far end sends is strobed into the input latch |
| `dav_bit` | enum | `none` | `none` \| `0` \| `1` \| `2` \| `3` \| `4` \| `5` \| `6` \| `7` | Data-bus bit a data-available flag is jumpered onto when this port is read (§3.2.2 status/data console): 0-7, or none |
| `dav_source` | enum | `self` | `self` \| `sibling` | Whose service request the dav_bit shows: self (this port's) or sibling (the other parallel port's -- the status/data console reads the data port's flag at the status port) |
| `dav_active_low` | bool | `false` | `on` \| `off` | Present the data-available flag active-low -- the dav_bit reads 0 when a byte is waiting (§3.2.2 "D0 going low"). Off = active-high |
| `int` | enum | `none` | `none` \| `int` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | Where this parallel input's interrupt (a byte latched in, service request) is strapped on header W4: none \| int \| vi0..vi7. Canonical: vi6 (Parallel A) / vi5 (Parallel B) *(interrupt strap)* |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "SSM IO-4 pa" |

#### Unit `pb` — `[board.unit.pb]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `connect` | string | `null` | text | The endpoint on this parallel port's line (CONNECT sets this): a file, socket, printer, in:/out: file, null. A WRITE latches out; a byte the far end sends is strobed into the input latch |
| `dav_bit` | enum | `none` | `none` \| `0` \| `1` \| `2` \| `3` \| `4` \| `5` \| `6` \| `7` | Data-bus bit a data-available flag is jumpered onto when this port is read (§3.2.2 status/data console): 0-7, or none |
| `dav_source` | enum | `self` | `self` \| `sibling` | Whose service request the dav_bit shows: self (this port's) or sibling (the other parallel port's -- the status/data console reads the data port's flag at the status port) |
| `dav_active_low` | bool | `false` | `on` \| `off` | Present the data-available flag active-low -- the dav_bit reads 0 when a byte is waiting (§3.2.2 "D0 going low"). Off = active-high |
| `int` | enum | `none` | `none` \| `int` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | Where this parallel input's interrupt (a byte latched in, service request) is strapped on header W4: none \| int \| vi0..vi7. Canonical: vi6 (Parallel A) / vi5 (Parallel B) *(interrupt strap)* |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "SSM IO-4 pb" |


## Video

### `pcvideo`

CompuPro (Viasyn) PC Video (board 229D): 16K of screen RAM, a Motorola 6845, an IBM PC keyboard port (unit kbd) and a light pen port, at the IBM Color Graphics Adapter's addresses: memory (default 0B8000, a 16K block) and ports (default 03D0, decoded on A0-A15). Port 0 enables the board (select, J3), port 1 moves it to the monochrome addresses (flip, J2), ports 4-5 the 6845, 8 the mode, 9 the color, 10 the status, 11-12 the keyboard. The keyboard and light pen pull a VI line (interrupt, default vi5). 80x25 and 40x25 text, 320x200 and 640x200 graphics in a window; monitor: color or mono (S2-9: 9x14 text cells, black-and-white graphics). font: the character generator (default builtin:pcvideo). lightpen: off or mouse

**Units:** `kbd` (serial, CONNECT)

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `memory` | int | `0xB8000` | `0x0` .. `0x3FC000` | S1-2..9: the 16K block of screen RAM, A21-A14 (default 0B8000, IBM's color address). A multiple of 4000 below 400000 |
| `ports` | int | `0x3D0` | `0x0` .. `0xFF0` | S2-1..8: the 16 ports, A11-A4 (default 03D0, IBM's color ports). A multiple of 10 below 1000 |
| `wom` | bool | `false` | `on` \| `off` | S1-1 OFF: write-only memory. The board takes writes; another RAM board at the same address answers reads (off: read and write) |
| `phantom` | bool | `false` | `on` \| `off` | S2-10 ON: pull PHANTOM* while the screen RAM is addressed (off, as normally set) |
| `monitor` | enum | `color` | `color` \| `mono` | S2-9: color (OFF: 14.31818 MHz, RGBI) or mono (ON: 16.257 MHz, 9x14 characters, underline and reverse, a green monitor) |
| `select` | int | `0` | `0` .. `7` | J3: the bit of port 0 that enables this board, 0-7 (default 0) |
| `flip` | bool | `true` | `on` \| `off` | J2 A-C: port 1 can move the board to the monochrome addresses (on). Off is J2 B-C |
| `interrupt` | enum | `vi5` | `none` \| `int` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | J4: the VI line the keyboard and light pen pull (vi5, CompuPro's standard) *(interrupt strap)* |
| `cursor_blink` | bool | `true` | `on` \| `off` | J5 installed: the cursor blinks at the board's rate (on). Off: a steady cursor |
| `font` | string | `builtin:pcvideo` | text | The character generator, U6 (a 2764): builtin:pcvideo (default), or a file (binary, Intel HEX or S-records) in the same layout |
| `lightpen` | enum | `off` | `off` \| `mouse` | The light pen: off (default), or mouse (the left button over the window is the pen and its switch) |
| `title` | string |  | text | The text on the window's title bar. Empty = "CompuPro PC Video" |
| `titlebar` | bool | `true` | `on` \| `off` | Show the title on a bar at the top of the window, in the board's own font |
| `width` | string | `auto` | text | Video window width in pixels: 'auto' (default) opens about half the screen wide, or a number like 1024. The height follows the board's own aspect, and the picture is a whole multiple of its pixels so it stays crisp |

#### Unit `kbd` — `[board.unit.kbd]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `connect` | string | `null` | text | The endpoint the keyboard's codes come from (CONNECT sets this) |
| `keys` | enum | `ascii` | `ascii` \| `scancodes` | What the endpoint's bytes are: ascii (characters, typed as a PC keyboard would; default) or scancodes (as the keyboard sends them) |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "CompuPro PC Video kbd" |


## Other

### `hostbridge`

Host Bridge: guest <-> host file transfer, sandboxed. OUR OWN BOARD, not a period one. Two ports at BASE+0..1. R.COM/W.COM/HDIR.COM

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `port` | int | `0xB0` | `0x0` .. `0xFE` | Base port. Two ports: BASE+0 command/status, BASE+1 data |
| `hostdir` | string |  | text | The sandbox root. Guest names resolve here and CANNOT escape it. Empty = the shell's working directory |
| `hostdir_root` | string | — | — | LIVE: the sandbox root as RESOLVED -- the actual directory the guest is fenced into. Read-only; `hostdir` is what was written. **(read-only — not a key you may set)** |
| `readonly` | bool | `false` | `on` \| `off` | Refuse OPEN_WRITE and DELETE -- the guest may read the host, not change it |


### `ss1`

CompuPro System Support 1: multifunction S-100 board. Dual 8259A interrupt controllers in a master/slave cascade (master/slave at base+0..+3; master watches VI0-6 and drives pin 73, slave takes the timer OUTs and the UART's Rx/TxRDY), an 8253 interval timer (three counters + control at base+4..+7, 2 MHz clock), the OKI MSM5832 battery-backed real-time clock/calendar (command/data at base+10/+11) and a 2651 UART serial channel (base+12..+15); base default 50H. The math socket (base+8/+9) takes an AMD 9511A or 9512 with `math`, and is empty by default

**Units:** `serial` (serial, CONNECT)

#### Board properties

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `base` | int | `0x50` | `0x0` .. `0xF0` | Base port of the 16-port I/O block. CompuPro standard is 50H |
| `waits` | enum | `0` | `0` \| `1` \| `2` \| `4` \| `8` | S1-1..4 wait states on every access to the board: 0 (shipped), 1 (W1, for a 4 MHz or faster CPU), 2 (W1+W2), 4 (+W4) or 8 (+W8). Math-chip and INTA cycles always get at least 2 |
| `math` | enum | `none` | `none` \| `9511a` \| `9512` | The chip in the math socket (U13): none (the stock empty socket), 9511a (AMD Am9511A, Intel 8231A) or 9512 (AMD Am9512, Intel 8232) |
| `memory` | bool | `false` | `on` \| `off` | S1-5 RDI: the 4K RAM block (U17 lower 2K, battery-backed; U16 upper 2K; both HM6116). false (default) = RDI ON, no memory |
| `memory_at` | int | `0xFF000` | `0x0` .. `0xFFF000` | S2 (A23-A16) and S3-1..4 (A15-A12): the 24-bit address of the 4K block, on a 4K boundary. Default 0FF000 (CompuPro's CP/M-86 setting) |
| `memory_global` | bool | `false` | `on` \| `off` | S1-6 XA: true = the 4K block ignores A16-A23 and answers in every 64K page; false (default) = it answers only at memory_at's A16-A23 |
| `memory_phantom` | enum | `disable` | `disable` \| `enable` \| `ignore` | S1-7 PHD / S1-8 PHE: what PHANTOM* does to the 4K block. disable (default, PHD ON), enable (PHE ON: on only while PHANTOM* is asserted) or ignore (both OFF) |
| `clock` | string | — | — | LIVE: the date/time the MSM5832 is showing, and its offset from host time **(read-only — not a key you may set)** |
| `timer` | string | — | — | LIVE: the three 8253 counters -- mode, current count and OUT level **(read-only — not a key you may set)** |
| `pic` | string | — | — | LIVE: the master/slave 8259A -- IRR/ISR/IMR and the master's INT pin **(read-only — not a key you may set)** |
| `mathchip` | string | — | — | LIVE: the math chip -- busy/idle, stack pointer, status, top of stack, END/SVREQ/ERR **(read-only — not a key you may set)** |

#### Unit `serial` — `[board.unit.serial]`

| Key | Kind | Default | Legal | Meaning |
|---|---|---|---|---|
| `baud` | int | `9600` | `50` .. `19200` | Line rate. The 2651 generates it from Mode Register 2, so the guest's MR2 write overwrites this; it seeds the line before then (min 50) |
| `interrupt` | enum | `none` | `none` \| `int` \| `vi0` \| `vi1` \| `vi2` \| `vi3` \| `vi4` \| `vi5` \| `vi6` \| `vi7` | Where this port's IRQ is jumpered: none \| int \| vi0..vi7. The chip raises it on RxRDY; the standard board routes it through the 8259A instead *(interrupt strap)* |
| `connect` | string | `null` | text | The endpoint on the other end of the line (CONNECT sets this) |
| `title` | string |  | text | The title of a terminal window on this unit. Empty = "CompuPro System Support 1 serial" |

