#include "boards/registry.h"

#include "boards/compupro-disk1.h"
#include "boards/compupro-disk2.h"
#include "boards/compupro-disk3.h"
#include "boards/compupro-mdriveh.h"
#include "boards/compupro-selch.h"
#include "boards/compupro-disk1a.h"
#include "boards/compupro-cpu8588.h"
#include "boards/compupro-cpu8687.h"
#include "boards/compupro-cpu286.h"
#include "boards/compupro-ss1.h"
#include "boards/hostbridge.h"
#include "boards/mits-z80cpu.h"
#include "boards/digicomp-pascal100.h"
#include "boards/mits-8085cpu.h"
#include "boards/s100-memory.h"
#include "boards/tarbell.h"
#include "boards/jade-dd.h"
#include "boards/compupro-pcvideo.h"
#include "boards/compupro-spuz.h"
#include "boards/io4.h"
#include "boards/compupro-interfacer1.h"
#include "boards/compupro-interfacer3.h"
#include "boards/compupro-interfacer4.h"
#include "boards/mits-2sio.h"

namespace altair {

// Milestone 1a is CLI + bus + memory, and NO CPU: the monitor is the bus master.
// That is not a limitation to apologize for -- it is the point. Every claim the
// bus design makes (a ROM that never answers a write, an empty socket that
// floats, a PHANTOM* overlay that is not contention, banked cards that each own
// their decode) is testable with two boards, a hex file, and no processor. And it is
// worth testing BEFORE a CPU exists, because those behaviors differ SILENTLY:
// get one wrong and the symptom is a guest misbehaving ten thousand
// instructions later.
// The type name is the CHIP, because that is the word an operator reaches for --
// `BOARDS ADD 8080 cpu0`. Nobody asks for an 88-CPU by its catalog number, and
// when the Z80 cards land they will be `z80`, which is what people called those
// too. The card's identity lives in its .md, where it belongs.
std::vector<BoardType> boardTypes() {
    return {
        {"memory", "RAM/ROM board: plain, unbanked memory regions", "RAM/ROM board: a list of regions and PHANTOM* -- plain, unbanked memory"},
        {"z80", "Generic Z80 CPU board", "Generic Z80 CPU board. Decodes nothing -- it drives the bus. The 88-CPU's twin, with a Z80 core"},
        {"pascal100", "Digicomp Pascal-100: Z80 + WD MicroEngine, one active", "Digicomp Research Pascal-100: a Z80 (unit z80) and the Western Digital MicroEngine (unit me, UCSD Pascal III.0 P-code), one active at a time. IEEE 696 bus only. System Control Port at PORT (switch U225, default D0) and the Map Output Port at PORT+1, both write-only. Standard Map: 16 entries of 8K, 128K physical, per processor on or off. Z80 and MicroEngine I/O put the port on both address halves; MicroEngine words FFpp/FEpp are I/O port pp. pINT and NMI go to the Z80; NMI, and pINT with auto-transfer on, give the Z80 control. Jump-on-reset (J202, reset_addr)"},
        {"cpu8588", "CompuPro CPU 8085/88: 8085 + 8088, one on line", "CompuPro CPU 8085/88 (161F): an 8085 (unit 8085) and an 8088 (unit 8088), one on line at a time; the 8085 starts. One port (PORT, switch S3, default FD): an IN swaps the processors and reads FF, an OUT latches the Memory Manager byte -- A16-A23 for the 8085, its high nibble on A20-A23 for the 8088. The 8088 first comes on line at FFFF0; after that each processor resumes, or is reset (swap_reset_8085/8088). Power-on-jump for the 8085 (power_on_jump, poj_addr). IOW: one wait state per I/O cycle. pINT reaches the processor on line; NMI and VI0-VI2 to the 8085's TRAP/RST pins are not modeled. IEEE 696 bus"},
        {"cpu8687", "CompuPro CPU 86/87: an 8086 on a 16-bit bus", "CompuPro CPU 86/87 (186B): an 8086 (unit 8086) with 8- and 16-bit IEEE 696 transfers (SIXTN*; byte-serial, even byte first, otherwise). Starts at FFFF0, or at poj_addr with power_on_jump. Its own ports, which decode A0-A15: the Memory Manager (mm_port, default 00FD: D7-D4 of an OUT = A23-A20) and a ROM-less 80130 (port_80130, default FFF0; io_80130): an 8259A-like interrupt controller and three timers in hard-wired modes 2, 0, 3, clocked by the CPU clock. The 80130's outputs (jumpers J0-J7) are not fitted. INT* (pin 73) and NMI* reach the 8086. io_wait: one wait state per I/O cycle. No 8087 (J8 fitted). Runs no 8-bit software. IEEE 696 bus"},
        {"cpu286", "CompuPro CPU 286: an 80286 on a 16-bit bus", "CompuPro CPU 286 (451B): an 80286 in real mode (unit 80286) with 8- and 16-bit IEEE 696 transfers (SIXTN*; byte-serial, even byte first, otherwise). Starts at FFFFF0. Its own ports, which decode A0-A15 (unless j2): the Memory Manager (a byte to an even port 01F8-01FE: D0-D3 = A20-A23, on the lines mm_lines says, J3-J6) and the 80287 ports 00F8-00FF (no 80287: reads float). sockets (J7): page FF from the on-board EPROM pair (rom). waits: jumper J1. No interrupt controller: INT* (pin 73) and NMI* reach the 80286. Runs no 8-bit software. IEEE 696 bus"},
        {"8085", "Generic 8085 CPU board", "Generic 8085 CPU board. Decodes nothing -- it drives the bus. The 88-CPU's twin, with an 8085 core (RIM/SIM + TRAP/RST 5.5/6.5/7.5)"},
        {"2sio", "MITS 88-2SIO: two 6850 serial ports", "MITS 88-2SIO: two 6850 ACIAs, units 'a' and 'b'. Four ports at BASE+0..3"},
        {"tarbell", "Tarbell #1011: single-density floppy controller", "Tarbell #1011: single-density WD FD1771 floppy, up to 4 drives. Eight ports at BASE+0..7 (default F8). Carries a 32-byte boot PROM that shadows 0000 over PHANTOM* -- boots CP/M automatically at reset (bootstrap=on)"},
        {"tarbelldd", "Tarbell #2022: double-density floppy controller", "Tarbell #2022: double-density WD FD1791 floppy (mixed-density media, SD track 0), up to 4 drives. The single-density card's twin with a bitmap OUT-FC latch and a port-FD DMA/ext-addr register. Same 32-byte boot PROM"},
        {"jadedd", "Jade Double D: double-density controller, own Z80", "Jade Double D Rev C: an intelligent double-density floppy controller with its own Z80A, 2 K RAM and WD FD1791. The host loads a program (Jade's DCM2) through a 1 K window at E000-FC00 and talks to it through one port (40-43, default 43). Up to 4 drives"},
        {"disk1", "CompuPro Disk 1: uPD765A floppy controller with DMA", "CompuPro Disk 1 (1981), the Disk 1A's predecessor: an NEC uPD765A floppy controller for 8\" drives, single and double density, up to 4 drives. Four ports at BASE+0..3 (default C0): 765 status, 765 data, board status / DMA address, and a bit-banged serial line on D7. All data moves by DMA. Carries a 2716 boot EPROM whose selected 256-byte routine answers at 0000 over PHANTOM* until the guest switches it off; any reset turns it back on"},
        {"disk1a", "CompuPro Disk 1A: uPD765A floppy controller with DMA", "CompuPro Disk 1A: an NEC uPD765A floppy controller for 8\" and 5.25\" drives, single and double density, up to 4 drives. Four ports at BASE+0..3 (default C0): 765 status / drive select, 765 data, board status / DMA address, motor control. All data moves by DMA. Carries a boot EPROM whose selected 512-byte routine answers at 0000 over PHANTOM* until the guest switches it off; any reset turns it back on"},
        {"disk2", "CompuPro Disk 2: hard disk controller (Fujitsu 2300), DMA", "CompuPro Disk 2 (board 177): a hard disk controller for up to four Fujitsu M2301B/M2302B drives (m10, m20: 244 cylinders, 4 or 8 heads, 11 sectors of 1024 bytes). Two ports at BASE+0..1 (default C8): CTL/STATUS, and the DRIVE/CYLINDER/HEAD/SECTOR registers (an IN steps the selected drive). Commands NULL, READ DATA, WRITE DATA, WRITE HEADER, READ HEADER. All data moves by DMA through a Selector Channel (`selch`) programmed for its dma_priority (default 10). ATTN to a VI line or pINT when a command ends"},
        {"disk3", "CompuPro Disk 3: intelligent ST506 hard disk controller, DMA", "CompuPro Disk 3 (board 194): an ST506 Winchester controller with its own 8085, up to four drives (default shape Quantum Q540: 512 cylinders, 8 heads, 9 sectors of 1024 bytes). One write-only attention port pair (default 90): 00 = attention, the J10 bit = reset. The host builds 16-byte IOPBs in memory, linked from 0:50h; the board fetches them, moves the data and writes STATUS back by its own DMA at dma_priority (default 9). Commands NOOP, VERSION, GLOBAL, SPECIFY, SET-MAP, HOME, SEEK, READ-HEADER, R/W, FORMAT, FORMAT-BAD. Completion interrupt to a VI line or pINT"},
        {"mdriveh", "CompuPro M-Drive/H: 512K RAM disk on two I/O ports", "CompuPro M-Drive/H (board 197B): 512K of dynamic RAM used as a disk drive, on two I/O ports (default C6H data, C7H address; S1-1..7 = A7-A1). Each OUT to the address port shifts a 22-bit counter up 8 bits and loads the byte; each data IN/OUT moves one byte and counts up. Up to eight boards share the same two ports: MA21-MA19 against `cascade` (S1-8..10) pick the board that stores and drives, and a range with no board reads FF. RAM contents at power-on per `fill`. CompuPro CP/M 2.2, CP/M-86 and MP/M 8-16 size the boards and use them as drive M:"},
        {"pcvideo", "CompuPro PC Video: 6845 color display, PC keyboard", "CompuPro (Viasyn) PC Video (board 229D): 16K of screen RAM, a Motorola 6845, an IBM PC keyboard port (unit kbd) and a light pen port, at the IBM Color Graphics Adapter's addresses: memory (default 0B8000, a 16K block) and ports (default 03D0, decoded on A0-A15). Port 0 enables the board (select, J3), port 1 moves it to the monochrome addresses (flip, J2), ports 4-5 the 6845, 8 the mode, 9 the color, 10 the status, 11-12 the keyboard. The keyboard and light pen pull a VI line (interrupt, default vi5). 80x25 and 40x25 text, 320x200 and 640x200 graphics in a window. Color mode only. font: the character generator (default builtin:pcvideo). lightpen: off or mouse"},
        {"spuz", "CompuPro SPUZ: slave processor, own Z80H", "CompuPro (Viasyn) SPUZ (board 257E): a slave processor with its own 8 MHz Z80H, 64K or 256K of RAM (memory) and a 2681 DUART with two RS-232 channels (units a and b). Not a bus master. To the host it is a 64K window (window = A23-A16, default 0E: 0E0000h) and ports FC30-FC41, decoded on A0-A15: FC30-FC3F the 2681, FC40 SLAVESEL (every board), FC41 PTLATCH (D3 ZRST*, D1-D0 host bank). Up to 16 boards share both; SLAVESEL against `select` (0-15) picks the one that answers. 2681 OP6 pulls a VI line (interrupt, default vi0) from reset until software sets OPR bit 6. An access while the Z80 is in reset hangs a real bus: here it floats and the run stops (bus-hang). waits: jumper J1"},
        {"selch", "CompuPro Selector Channel: the Disk 2's DMA cycles", "CompuPro Selector Channel (board 172): the 24-bit address, status and strobes of a temporary master, shared by the controllers that use it (the Disk 2). One port (default F0): an IN disables it, then four OUTs load A23-A16, A15-A8, A7-A0 and the MODE byte (write/read, memory/I/O, up/down, wait, and the complement of the controller's DMA priority). The boot EPROM is not modeled"},
        // "CONNECT it to a file" was true when a file was the only place printing could go.
        // It is not the offer any more -- printer:, socket: and the rest are on the same
        // list HELP CONNECT prints -- and a one-liner that names one endpoint reads as if
        // it named the only one.
        // Not a toggle: the SENSE switches a guest reads at IN 0FFH are a CONFIGURED byte
        // (SET fp0 sense=, or TOML) -- there is no switch on this board to flip. No OUT.
        {"ss1", "CompuPro System Support 1: interrupts, timer, clock, serial", "CompuPro System Support 1: multifunction S-100 board. Dual 8259A interrupt controllers in a master/slave cascade (master/slave at base+0..+3; master watches VI0-6 and drives pin 73, slave takes the timer OUTs and the UART's Rx/TxRDY), an 8253 interval timer (three counters + control at base+4..+7, 2 MHz clock), the OKI MSM5832 battery-backed real-time clock/calendar (command/data at base+10/+11) and a 2651 UART serial channel (base+12..+15); base default 50H. The math socket (base+8/+9) takes an AMD 9511A or 9512 with `math`, and is empty by default"},
        {"hostbridge", "Host Bridge: file transfer to the host (not a period board)", "Host Bridge: guest <-> host file transfer, sandboxed. OUR OWN BOARD, not a period one. Two ports at BASE+0..1. R.COM/W.COM/HDIR.COM"},
        {"interfacer1", "CompuPro Interfacer 1: two serial channels", "CompuPro Interfacer 1 (Godbout 133H): two serial channels 'a' and 'b', each a real 1602-family UART at its own 2-port block (switch S2/S3: port, enabled; default A at 00-01, B at 02-03), data at the even address and status at the odd one unless J14 swaps them (swap_ports). An OUT to the status address writes a control latch: each 1 bit inverts that line from its power-up strap and each 0 returns it, for the word format (data_bits 7/8, parity_sense, parity, stop_bits), the RS-232 outputs ca (RTS) and cd (DTR), and the two interrupt enables; reset clears the latch. The status byte is TBMT D0, DAV D1, OPT D2 (opt = cf or eoc), PE D3, OR D4, FE D5, CC D6, CB D7; the RS-232 inputs read 0 when ON. Per channel, RxINT (DAV and the RX enable) and TxINT (TBMT and the TX enable) are jumpered on J11 to vi0..vi7 or none (rx_int/tx_int); J11 has no pin-73 position. Baud from switch S1, 50-19200. CONNECT each channel to a file, socket, serial port, in:/out:"},
        {"interfacer3", "CompuPro Interfacer 3: eight serial channels", "CompuPro Interfacer 3 (board 174): eight serial channels user0..user7, each a 2651 USART, behind one 8-port block (port, a multiple of 8, default 10H; enabled = S1-1). Relative ports 0-3 are the selected USART's data, status, mode and command; 4 and 5 read TxRDY0-7 / RxRDY0-7 and write the transmit / receive interrupt masks; 6-7 write the user select latch (D0-D2 user, D3-D4 board). Every board at the same port latches every select write and answers only while D3-D4 match its users switch (0-7, 8-15, 16-23, 24-31), so up to four boards share one block. Per user, TxINT (TxRDY and its mask bit) and RxINT (RxRDY and its mask bit) are jumpered to vi0..vi7 or none (tx_int/rx_int); reset clears the masks and the latch. waits (J17) adds 0-2 wait states to USART accesses. Unconnected RS-232 inputs read ON. CONNECT each user to a file, socket, serial port, in:/out:"},
        {"interfacer4", "CompuPro Interfacer 4: three serial, Centronics, parallel", "CompuPro Interfacer 4 (board 187C): four users behind the Interfacer 3's 8-port block (port, a multiple of 8, default 10H; enabled = S2-4). users (S2-1..3) picks the four: half of a group of eight, so two boards make a group (default 4-7). Relative user 0 = the parallel ports: port 0 writes the Centronics printer (unit centronics) and reads the sense switch S1 (sense), port 1 reads Status-C and writes Control-C, port 2 is the Universal Parallel data (unit parallel), port 3 reads Status-P. Users 1-3 (units user1..user3) are 2651 USARTs. Ports 4/5 read this board's nibble of Tx/Rx ready and write its nibble of the interrupt masks; 6-7 write the user select latch, which every board at the port takes. tx_int/rx_int jumper each source to vi0..vi7. j26 swaps users 0 and 2; waits (J6-J8) adds 0-3 wait states. CONNECT centronics to out:FILE or printer:QUEUE"},
        {"io4", "SSM IO-4: two serial + two parallel ports", "SSM IO-4 (2P+2S): the real Solid State Music board -- two full-duplex serial channels AND a four-port parallel section. Serial units 'a'/'b' are real 1602-family UARTs with programmable word length (data_bits 5-8), parity and stop bits, plus the full status-word strap-up (stat_* map, invert_status, port_reversal) and named profiles (default altair-rev1). A 4-port block set by switch S3 (default 0-3): Serial A status/data at BASE+0/+1, Serial B at BASE+2/+3. Parallel units 'pa'/'pb' are 8212 latched ports (input latch + service request, output latch) on their own 2-port block set by switch S4 (par_port, default 4-5): Parallel A at PAR+0, B at PAR+1; a byte the far end sends is strobed in, a write latches out, and the §3.2.2 status/data console flag is strappable (dav_bit/dav_source/dav_active_low). If the two blocks overlap, neither section answers the shared ports. Each serial channel's receive (DAV) and transmit (TBMT) and each parallel input (service request) can raise an interrupt, strapped on header W4 to a VI line, pin 73 (int), or none (rx_int/tx_int per serial channel, int per parallel port) -- there is no software enable, the strap is the enable, and a parallel input interrupt rises even when the port is not addressed. Configure each unit under [board.unit.a] / [board.unit.pa] etc.; CONNECT each to a file, socket, serial port, in:/out:"},
    };
}

static std::unique_ptr<Board> buildBoard(const std::string& typeName) {
    // A board type is a name the operator typed, so `2SIO` is `2sio` (core/board.h).
    const std::string type = lowerAscii(typeName);
    if (type == "memory") return std::make_unique<MemoryBoard>();
    if (type == "z80") return std::make_unique<CpuZ80Board>();
    if (type == "pascal100") return std::make_unique<Pascal100Board>();
    if (type == "cpu8588") return std::make_unique<Cpu8588Board>();
    if (type == "cpu8687") return std::make_unique<CompuProCpu8687Board>();
    if (type == "cpu286") return std::make_unique<CompuProCpu286Board>();
    if (type == "8085") return std::make_unique<Cpu8085Board>();
    if (type == "2sio") return std::make_unique<Sio2Board>();
    if (type == "tarbell") return std::make_unique<TarbellBoard>();
    if (type == "tarbelldd") return std::make_unique<TarbellDdBoard>();
    if (type == "jadedd") return std::make_unique<JadeDdBoard>();
    if (type == "pcvideo") return std::make_unique<PcVideoBoard>();
    if (type == "spuz") return std::make_unique<SpuzBoard>();
    if (type == "ss1") return std::make_unique<Ss1Board>();
    if (type == "disk1") return std::make_unique<CompuProDisk1Board>();
    if (type == "disk1a") return std::make_unique<CompuProDisk1aBoard>();
    if (type == "disk2") return std::make_unique<CompuProDisk2Board>();
    if (type == "disk3") return std::make_unique<CompuProDisk3Board>();
    if (type == "mdriveh") return std::make_unique<CompuProMDriveHBoard>();
    if (type == "selch") return std::make_unique<SelectorChannelBoard>();
    if (type == "hostbridge") return std::make_unique<HostBridgeBoard>();
    if (type == "io4") return std::make_unique<Io4Board>();
    if (type == "interfacer1") return std::make_unique<InterfacerOneBoard>();
    if (type == "interfacer3") return std::make_unique<InterfacerThreeBoard>();
    if (type == "interfacer4") return std::make_unique<InterfacerFourBoard>();
    return nullptr;
}

// The one factory. The catalog line names the product ("CompuPro Interfacer 4: three
// serial, ..." -> "CompuPro Interfacer 4"), and the board keeps that name for the titles
// of its windows (Board::defaultUnitTitle).
std::unique_ptr<Board> makeBoard(const std::string& typeName) {
    auto b = buildBoard(typeName);
    if (!b) return b;
    const std::string type = lowerAscii(typeName);
    for (const auto& bt : boardTypes())
        if (bt.name == type) {
            const auto colon = bt.summary.find(':');
            b->setProductName(colon == std::string::npos ? bt.summary : bt.summary.substr(0, colon));
            break;
        }
    return b;
}

} // namespace altair
