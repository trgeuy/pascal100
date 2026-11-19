# CompuPro Interfacer 4

**Status:** done for the three serial channels, the Centronics port and the Universal Parallel
port as software sees them. Synchronous mode, the current loop, the TxEMT/DSCHG jumpers and the
parallel strobe polarities are not modeled (see Limitations).

## The real hardware

The Interfacer 4 (CompuPro, a Godbout company, board 187C, manual dated 5/83) holds three 2651
USARTs, a Centronics printer port and a 16-line Universal Parallel port. It uses the Interfacer
3's 8-port block and user select latch, so the two boards mix at the same ports.

One board holds four users, half of a group of eight. Relative user 0 is the two parallel
ports. Relative users 1, 2 and 3 are the 2651s U36, U35 and U34 (connector 3 C, B and A). Two
boards, one for each half, make a group.

Configuration in period:

- **S2-4:** ON takes the board off the bus.
- **S2-5..9:** A7-A3 of the block. ON is 0. CompuPro software uses 10H.
- **S2-1, S2-2:** the group of eight users. **S2-3:** the half. OFF is the high four.
- **S3:** which nibble of the status and mask ports the board uses. It must agree with S2-3.
- **JS5, JS6:** each transmit and receive interrupt, to any VI line.
- **J6, J7, J8:** 1, 2 or 3 wait states on ports 0-3.
- **J26:** A-B/C-D puts the parallel ports at relative user 0 (standard). A-C/B-D swaps them
  with the middle serial channel.
- **S1-3..10:** a sense switch that software can read.
- **J2-J5:** the power-up levels of the printer control lines, and the printer interrupt source.

## Sources

| Source | Path | Authority |
|---|---|---|
| *Interfacer 4 Technical Manual*, CompuPro, 5/83, board 187C, document #11877, 58 pp. with the logic diagram | `reference/CompuPro Interfacer 4.md` (converted from `CompuPro Interfacer 4 Manual.pdf`) | Every fact on this page. Ports p. 8, user select pp. 9-10, S3 p. 10, J26 p. 11, interrupts pp. 13-15, Universal Parallel pp. 22-24, Centronics pp. 25-28, logic diagram pp. 50-57 |
| *MP/M 8-16 Technical Manual and Installation Procedures*, CompuPro, March 1984 | `Documentation/CompuPro/MPM_Technical_Manual_and_Installation_Mar84.pdf` (project folder) | The jumpers MP/M expects: JS5 TxINT 4-7 to VI3, JS6 RxINT 4-7 to VI2, J26 A-B/C-D |

The text of the manual leaves out how two boards share ports 4 and 5. The logic diagram shows it:

- USEL compares only D3-D4 of the latch, so both boards of a group are selected.
- The 2651 chip enables and the full-byte data enable add the D2 (half) match. So ports 0-3
  answer on one board.
- On ports 4 and 5, each board drives only its own nibble (DOENL\* or DOENH\*).

The manual has about a dozen errors; the conversion marks each with [sic]. Two matter here:

- Page 22 calls Status-P D1 "data taken". The logic diagram makes it "data not taken": a write
  sets it.
- Page 58 names S1-1 the strobe polarity. It is the output enable polarity; S1-2 is the strobe.

## Register reference

Eight ports at `port` (a multiple of 8). "Selected" means the user in D0-D1 of the user select
latch. Ports 0-3 answer only while D2-D4 of the latch match the board's `users` switch. Ports 4
and 5 answer while D3-D4 match.

| Relative | OUT (write) | IN (read) |
|---|---|---|
| 0 | 2651 data; on the parallel user, Centronics data | 2651 data; on the parallel user, the sense switch S1-3..10 |
| 1 | 2651 SYN/DLE; on the parallel user, Control-C | 2651 status; on the parallel user, Status-C |
| 2 | 2651 mode; on the parallel user, Universal Parallel data | 2651 mode; on the parallel user, the Universal Parallel input latch |
| 3 | 2651 command; on the parallel user, nothing | 2651 command; on the parallel user, Status-P (D0-D1 only) |
| 4 | This board's nibble of the transmit interrupt mask | This board's nibble of the transmit ready bits |
| 5 | This board's nibble of the receive interrupt mask | This board's nibble of the receive ready bits |
| 6, 7 | User select: D0-D1 user, D2 half, D3-D4 group | Not decoded |

Nibble bits, on the low nibble (a 0-3 board) or the high nibble (a 4-7 board):

| Bit | Transmit (port 4) | Receive (port 5) |
|---|---|---|
| 0 | TxI4: the printer acknowledged | RxI4: the Universal Parallel port latched a byte |
| 1 | TxRDY of user 1 (U36) | RxRDY of user 1 |
| 2 | TxRDY of user 2 (U35) | RxRDY of user 2 |
| 3 | TxRDY of user 3 (U34) | RxRDY of user 3 |

Status-C: D0 not busy, D1 0, D2 ACKNLG\*, D3 paper out, D4 and D5 error, D6 selected, D7 not
busy. A ready printer reads `C5`. Control-C: D0 auto feed, D1 init, D2 select in, D3 LED.
Status-P: D0 data available, D1 data not taken.

## How it is simulated

- **Units:** `centronics`, `parallel`, and `user1` to `user3`. The USARTs are the `Sig2651`
  chip of the Interfacer 3 and the System Support 1.
- **The user select latch** is taken in `snoop()`, as on the Interfacer 3. Every board at the
  port latches each select write. The select write itself is claimed by the board of the
  selected half.
- **Ports 4 and 5:** both boards of the group decode a read. Each says it drives only its own
  nibble (`Board::drivesBits`). The bus ORs the nibbles and floats a nibble with no board to
  1s, so a single 4-7 board with nothing pending reads `0F` (manual page 15). It is not
  contention (DESIGN.md 4.6). A mask write is latched in `snoop()` by both boards of the group.
  Each board takes its own nibble.
- **The Centronics port:** a data write goes to the endpoint of the
  `centronics` unit: an `out:` file, a `printer:` queue, a socket, the console. The line is
  raw: no byte is changed. After each write the printer handshake takes about 20 µs:
  - The port reads busy for the whole handshake.
  - ACKNLG\* reads low for the last 10 µs.
  - TxI4 sets at the end, where ACKNLG\* goes high again.
  A data write clears TxI4. Paper out, error and not selected never occur.
- **The Universal Parallel port:** a write goes to the `parallel` unit's endpoint, and the
  stream takes it at once, so "data not taken" clears when the stream can accept the byte. A
  byte that arrives on the stream latches as a strobe would: data available, RxI4. The next byte
  waits until the guest reads this one.
- **Interrupts:** each source is its ready bit AND its mask bit, to the VI line in `tx_int` or
  `rx_int`. The Centronics unit has `tx_int` (bit 0 of port 4); the parallel unit has `rx_int`
  (bit 0 of port 5).
- **J26** swaps the registers of relative users 0 and 2, and nothing else. Their status and mask
  bits stay where the chips are wired.
- **Wait states:** `waits` adds 0-3 wait states to each access of ports 0-3.

Properties:

| Property | Switch | Default | Meaning |
|---|---|---|---|
| `port` | S2-5..9 | 10 | The base of the 8-port block |
| `enabled` | S2-4 | on | The board answers |
| `users` | S2-1..3 (and S3) | 4-7 | The four users the board answers |
| `waits` | J6/J7/J8 | 0 | Wait states on ports 0-3 |
| `j26` | J26 | ab-cd | ab-cd: parallel ports at relative user 0. ac-bd: at relative user 2 |
| `sense` | S1-3..10 | FF | The sense switch: bit 7 = S1-3, bit 0 = S1-10. ON reads 0 |
| `select`, `tx_mask`, `rx_mask`, `control` | | | Read-only: the latch, this board's mask nibbles, Control-C |
| `centronics.connect`, `parallel.connect` | | (none) | The endpoint on each parallel port |
| `centronics.tx_int`, `parallel.rx_int` | JS5, JS6 | none | The VI line for the parallel interrupts |
| `user<n>.baud`, `user<n>.connect`, `user<n>.tx_int`, `user<n>.rx_int` | JS5, JS6 | 9600, null, none, none | As on the Interfacer 3 |

### Reset

`Reset::Bus` (CLR) clears the user select latch, both masks, Control-C (so the LED goes off),
the input flop, and each USART. It does not clear TxI4: on the real board the only clear of that
flop is a data write. `Reset::PowerOn` also clears TxI4 and "data not taken"; see Limitations.

## Quirks reproduced

| Quirk | If you get it wrong |
|---|---|
| Both boards of a group answer ports 4 and 5, one nibble each | A driver reads only half the users' status, or the bus reports contention on every interrupt |
| A nibble with no board reads 1s | Software that tests "no interrupts" against `F0` or `0F` hangs |
| J26 moves the registers, not the status bits | An interrupt-driven system answers the wrong user; the manual requires A-B/C-D under MP/M |
| Reset does not clear TxI4 | A driver that waits for the acknowledge after reset waits forever on real hardware |
| Status-P drives D0-D1 only | D2-D7 read 1, not 0 |
| A0 is not decoded at the user select port | A driver that writes the select to port 6 changes nothing |

## Limitations and deliberate departures

- **TxI4 at power-up.** The logic diagram gives the acknowledge flop U16b no power-up clear, so
  its first state is not defined. This model starts it clear. MP/M 8-16 polls the printer status
  and does not depend on it.
- **The printer is always ready.** Paper out, error and not selected never occur. The J2-J4
  power-up levels of the control lines, and their effect on a printer, are not modeled; Control-C
  is a latch that software can write and SHOW can read.
- **J5 (interrupt on ACKNLG\* or on BUSY)** is not a property: in this model both edges fall at
  the same moment.
- **The Universal Parallel port** has no strobe or output enable polarity (S1-1, S1-2) and no
  attention line (J1). A stream has none of those lines.
- **Synchronous mode** (and the U55 clock divider, J13-J24), **the current loop** (JS4, J25)
  and **TxEMT/DSCHG** (J10-J12) are not modeled. A write to port 1 of a serial user is ignored.
- **S3** is not a separate property. It follows S2-3, as page 10 requires; a mismatch "will
  cause improper board operation and possible board damage".
- **A mask write when the selected half has no board** is claimed by nobody. Through `snoop()`,
  the board of the other half still latches its nibble, as on the real bus; only the bus's
  "unclaimed" report differs.

## Verification

`tests/test_interfacer4.cpp` (98 checks): two Interfacer 4s at 10H (users 0-3 and 4-7) and an
Interfacer 3 at the same port for users 8-15.

- Every board latches the select; ports 0-3 answer on one board, ports 4/5 on both boards of the
  group; the Interfacer 3 alone answers its group. Port 16 selects as port 17 does.
- Port 14 combines the two nibbles without contention; a write of `52` loads `2` and `5` into the
  two masks (the manual's page 14 example).
- A single board reads `0F` on port 15 and `FC` on Status-P.
- The Centronics port: `C5` when ready, the byte on the line, busy during the handshake, TxI4 at
  its end, TxINT on the jumpered VI line, cleared by the next data write, Control-C latched and
  cleared by reset, TxI4 kept through reset.
- The sense switch (the page 28 example), the Universal Parallel input and output, and RxINT.
- The serial channels; J26 moves the Centronics port to user 6 and leaves its TxI on bit 4.
- An Interfacer 3 and an Interfacer 4 in one group both drive D0-D3: that is still contention.
- Snapshot, CONFIG SAVE, and a port that is not a multiple of 8.

Period software: MP/M 8-16 2.1E with two Interfacer 4s at 10H (users 0-7) and an Interfacer 3
(users 8-15), every TxINT on VI3 and RxINT on VI2. tty1 (user 7, the second board's user 3) runs
under `logout`. `PIP LST:=LPRS` and `PIP LST:=TTYS` print through the Centronics port of user 4
to an `out:` file, byte for byte. On this disk printer 0 (the default) is user 4 and `PRINTER 1`
is user 5, the reverse of the comments in its LPRS file; the Interfacer 3 machine shows the same.

## References

- `reference/CompuPro Interfacer 4.md`
- `docs/boards/compupro-interfacer3.md`, the board it shares ports with.
