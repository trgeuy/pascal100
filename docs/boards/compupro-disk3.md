# CompuPro DISK 3 (`disk3`)

**Status:** done for what CompuPro's CP/M 8-16 V1.1PD uses. CompuPro's `DISK3.CMD` formats a
Quantum Q540 image with 0 errors. `CPM340.SYS` boots from a floppy, finds the board, and reads
and writes all five hard disk partitions (A: to E:). See *Limitations*.

## The real hardware

The CompuPro (Viasyn) DISK 3, board 194: an intelligent Winchester controller for up to four
ST506 drives. An 8085 on the board does all the disk work. The host talks to it through memory:
it builds a 16-byte **IOPB** (Input Output Parameter Block) and writes the attention port. The
board fetches the IOPB, moves the data and writes the status back, all by its own DMA.

CompuPro sold it with the Quantum **Q540** (40 MB): 512 cylinders, 8 heads, 9 sectors of
1024 bytes a track.

Switches and jumpers: S1-1..7 port A7-A1 (90H), J11 P0-P3 DMA priority (9), J10 the reset bit,
J9 the completion interrupt (VI0-VI7 or INT).

## Sources

| Source | Path | Authority |
|---|---|---|
| DISK 3 Technical Manual, May 1985 (D-revision firmware) | `Reference/compupro/disk3.md` | The port, the IOPB, the commands, the status codes, SPECIFY, the disk structure. |
| CompuPro CP/M 8-16 V1.1PD BIOS, 1983 (`GBCBIOS2.A86`, `GBCBIOSA.A86`, `GBCBIOS6.A86`, `GBCBIOS1.A86`) | (period source) | Port 90, reset 01, the warm-up attention, the R/W IOPB, the Q540 partitions and "22 tracks reserved by controller". |
| CompuPro `DISK3.CMD` (10/13/83) | (period binary) | What a formatted disk holds: the label, the specify block with RES-TRACKS = 21, 21 map entries. |

## Interface

| Port | OUT | IN |
|---|---|---|
| 90 or 91 | 00 = attention. A byte with the J10 bit set (default bit 0) = reset. Other bytes: another board's reset. | Nothing (the board does not drive the bus). |

A reset (RESET\* on the bus, or the J10 bit at the port) restarts the firmware with its defaults:
absolute sector mode, the drive's own shape with no reserved tracks, no map (manual p. 17-18:
"Reset the board by pulsing the reset bit in the attention port", and "the DISK 3 firmware
defaults to ST506 parameters and absolute sector mode after a reset"). So an operating system
that resets the board can read absolute sectors 0 and 1, even after a boot EPROM left GLOBAL
mode 0 and a SPECIFY behind.

The first attention after a reset only fetches the LINK (bytes 13-15) of the IOPB at 0:50h.
Each later attention fetches the IOPB at the last LINK and runs it. STATUS (byte 1) is written
last: 0 = busy, FF = done, 01-08 = an error (manual p. 12).

Commands: NOOP, VERSION, GLOBAL, SPECIFY, SET-MAP, HOME, SEEK, READ-HEADER, R/W, FORMAT,
FORMAT-BAD. Bit 6 chains to the next IOPB. Bit 7 asks for the interrupt.

## How it is simulated

- **DMA.** The board pulls pHOLD at `dma_priority` and moves one byte per granted bus cycle with
  a 24-bit memory cycle: the IOPB fetch, the data and the write-back.
- **The disk image** is the drive's physical sectors, cylinder by cylinder, head by head,
  sector 0 first (a raw `.DSK`). Its shape is set per drive with `cylinders`, `heads`, `sectors`
  and `sector_size` (default Q540). An empty file becomes a new disk filled with E5.
- **Logical tracks.** A logical track is moved past the label track and the RES-TRACKS
  reserved tracks, unless the SET-MAP map sends it to a reserved track.
- **Time.** HOME, SEEK and the data take no emulated time.

## Limitations

- **The data offset is RES-TRACKS + 1. This is an inference.** The manual says RES-TRACKS
  counts the label track. But its Q540 example, `DISK3.CMD` (RES-TRACKS = 21) and the BIOS
  ("22 tracks reserved") all put the first data track at physical track 22. With these
  22 tracks, the V1.1PD partitions start on cylinders 3, 75 and 495, and E: ends on the last
  track of the drive. When RES-TRACKS is 0 (a drive being formatted), the offset is 0.
- **Defaults after a reset.** The manual says the firmware "defaults to ST506 parameters"
  but does not give them. The model uses the drive's own shape with no reserved tracks.
- **VERSION** answers 0D. The manual does not give the number.
- **RELOCATE, STATUS, SELECT, EXAMINE and MODIFY are not modelled.** They answer STATUS 01 and
  write a line to the log. The manual does not describe the drive status bits or the 8085's
  memory well enough to answer them truthfully. A good image never fails a read, so the BIOS
  does not send RELOCATE.
- **READ-HEADER** writes the cylinder high byte first, then head 0, sector 0. The manual does
  not give the byte order.
- **FORMAT-BAD** fills the track with 4E. On a real drive, a later read of the track gives
  TIME OUT. In the model the 4E bytes read back.
