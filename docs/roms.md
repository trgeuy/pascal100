# Built-in ROMs

The common ROMs are **compiled into the simulator**. There is no portable place to keep ROM images across macOS, Linux, and Windows — `/usr/share`, `~/Library`, `%APPDATA%`, and "next to the binary" are four different answers and every one of them becomes a support question. ROMs are small, so they are embedded and the question disappears: a fresh checkout boots on any OS with nothing to download.

Use one from a `memory` board region:

```toml
  [[board.region]]
  type  = "rom"
  at    = 0xF000
  mount = "builtin:ssm-8080mon"
```

`SHOW ROMS` lists what is compiled in. A bare path (`mount = "roms/mine.bin"`) loads a host file instead — **built-ins are a convenience, never a lock-in.**

## The provenance rule

`DESIGN.md` §0.1 says hardware facts come from period manuals and first-hand artifacts, never from another emulator. **A ROM image is a hardware fact**, and an embedded blob with no recorded source is exactly the second-hand fact that rule exists to prevent — worse than a wrong bit in a document, because every piece of software above it would then be debugged against the wrong ground truth, and it would look like a software bug for a very long time.

So **every built-in ROM has a row in the table below**, and no ROM is embedded without one:

| Field | Why |
|---|---|
| **Source** | A specific dump, part, or listing. "From the internet" is not a source. |
| **Size** | Exact bytes. A ROM padded to the next power of two is a *different artifact*. |
| **CRC32** | Verified by a unit test at build time, so a corrupted embed **fails the build**, not a user. |
| **Verified against** | The listing, manual, or hardware the dump was checked against — if it was. Say so honestly when it wasn't. |

## The ROMs

| `builtin:` name | Part | Size | CRC32 | Source | Verified against |
|---|---|---|---|---|---|
| `ssm-8080mon` | SSM 8080 System Monitor V1.0 (C. E. Ohme) | **2008 B** programmed in the **2 KB** `F000`–`F7FF` window (gaps `F5EE`–`F5FF` and `F6EA`–`F6FF` read `FF`) | **`27636BB0`** (over the `FF`-filled span) | `roms/SSM-8080MON/` — `SSM8080.ASM` / `SSMSCP.ASM` / `SSMVID.ASM` (+ their `.PRN`) and the combined `SSM-8080MON.HEX`. **C. E. Ohme, SSM Microcomputer Products** (formerly Solid State Music); OCR'd from the printed listing by **B. Beech**, Apr 2014. One 2K EPROM built from three independent modules — the monitor (`F000`), the System Configuration Package/driver tables (`F600`) and the VB1 video driver (`F700`) — assembled with **M80/L80** under CP/M inside the simulator (recipe in `docs/devguide/assembling-roms.md`). This is the SSM PB1 manual's `MONIT` (`F009h` = console-out, `F021h` = warm re-entry). Console on the strap-configurable serial board, ports `0`/`1`. | Its own three `.PRN` listings (byte-for-byte over the programmed span). **And it boots**: mounted at `F000` on a Z80 with an `io4` console on channel `a`, it reaches the `MONITOR V1.0` banner and its command prompt, and the `X` (register) command shows the registers (`tests/test_io4.cpp` edits eight and reads them back). |
| `tarbell-sd` | Tarbell floppy controller boot PROM | **32 B** (`0000`–`001F`) | **`55E3446D`** | `roms/TARBELL-SD/` — `TARPROM.ASM` / `TARPROM.HEX`. The **Tarbell Electronics** single-density floppy interface (#1011, 1977) shipped a 32-byte bootstrap PROM that shadows low memory over PHANTOM\*: it homes the head, reads **track 0, sector 1 into `0000h`** through the FD1771 with a wait-state (PRDY) transfer, then jumps to `07Dh` — an address with A5 set, which combinationally releases the shadow and drops into the cold loader it just read. The same PROM boots the double-density card (#2022). Distilled in `reference/Tarbell_Floppy_Disk_Interface_Manual.md`. | Its own `TARPROM.ASM` source, which is in the repository — the 32 bytes are the whole part. **And it boots**: the test machines of `acceptance-tarbell` and `acceptance-tarbelldd` load CP/M 2.2 off the tracked disks and reach `A>` through this PROM. |
| `ddboot` | Jade Double D boot PROM (DDBOOT, CP/M 2.2 Release 2) | **581 B** programmed in the 590-byte span `F000`–`F24D` (gap `F1E3`–`F1EB` reads `FF`) | **`631C3AD5`** (over the `FF`-filled span) | `roms/DDBOOT/` — `DDBOOT.ASM` / `DDBOOT.HEX` / `DDBOOT.PRN`. The **Jade Computer Products** host boot PROM for the **Jade Double D** controller (`jadedd`). It finds the board's 1 K window from port `43h`, copies a 200-byte program into the board's RAM, starts the board Z80 with `OUT 43h, 80h`, and copies the loaded system into host memory. Jade's BIOS takes its console from the PROM (`F006`/`F009`/`F00C`), at ports `00`/`01` as Jade delivered it (bit 1 = input ready, bit 2 = output ready). That is the port map of the SD Systems VDB-8024. The Jade test machines use `ddboot-2sio`. Jade's own `DDBOOT.ASM`, from the Jade CP/M 2.2 master disk S/N 2-187-01961; distilled in `roms/DDBOOT/README.md`. | Its own `DDBOOT.PRN` listing, byte-for-byte: DRI `ASM` 2.0, run inside the simulator (then altairsim), makes `DDBOOT.HEX` and `DDBOOT.PRN` from the source. |
| `ddboot-2sio` | Jade Double D boot PROM, patched for an 88-2SIO console | **581 B** programmed in the 590-byte span `F000`–`F24D` (gap `F1E3`–`F1EB` reads `FF`) | **`A017E174`** (over the `FF`-filled span) | `roms/DDBOOT-2SIO/` — `DDBOOT2.ASM` / `DDBOOT2.HEX` / `DDBOOT2.PRN`. Jade's `ddboot`, patched only at the places Jade's header marks for the owner's console: `INIT` resets the 6850 and sets 8N1, divide by 16, and the console is an **88-2SIO** at `10`/`11` (bit 0 = RDRF, bit 1 = TDRE). Every address is the same as in `ddboot`. The Jade test machines boot with it. The patch (`ddboot2.asm`, 2020-04-30) is in the maintainer's collection; distilled in `roms/DDBOOT-2SIO/README.md`. | The maintainer's 2020 `DDBOOT2.HEX` and `DDBOOT2.PRN`, byte-for-byte: DRI `ASM` 2.0, run inside the simulator (then altairsim), makes both from the source. **And it boots**: the test machine `tests/acceptance/jadedd.toml` loads Jade CP/M 2.2 to `A>` through it (`acceptance-jadedd`, `acceptance-jadedd-dd`). |
| `disk1` | CompuPro Disk 1 boot EPROM (2716) | **2048 B** (`0000`–`07FF`) — eight **256-byte** routine pages; the board shows the one that S2-1..2 and J17 (`routine`) pick at `0000`–`00FF` | **`EBA8EE41`** | `roms/DISK1/` — `DISK1.HEX`. **CompuPro**, a dump of a factory Disk 1 EPROM: routines 4–7 for the 8080/8085/Z80, and 0, 2 and 3 (and a copy in 1) for the 8086. Routines 4–7 are one routine that passes a different console code in register C. Fetched 2026-10-15 from deramp.com (`mfe_archive/…/171 Disk 1/CompuPro_Disk1_80_86.BIN`). | Routine 4 disassembled in the simulator and compared with the read sequence of the *Disk 1 Technical Manual* and of CompuPro's `HMXFPROM.ASM`: the same ports, DMA address and Specify. **And it boots**: `pascal100 disk1` loads CompuPro CP/M 2.2N (`CompuPro Systems 64K CP/M 2.2N`) off an 8″ disk through routine 4 and reaches `A>`. Routine 6 boots it to `A>` on the System Support 1. |
| `disk1a` | CompuPro Disk 1A boot EPROM (2764) | **8192 B** (`0000`–`1FFF`) — sixteen **512-byte** boot routines; the board shows the one that S1-2..5 (`routine`) picks at `0000`–`01FF` | **`93DBAEF4`** | `roms/DISK1A/` — `291B_1.HEX`. **CompuPro**, the Disk 1A's boot EPROM: routines 0–3 for the 8080/8085/Z80, 4–7 for the 8086, 8–11 for the 68000, 12–15 for the 32016. Fetched 2026-10-14 from Herb Johnson's ROM collection (retrotechnology.com, `herbs_stuff/roms/291B_1.HEX`). | Routine 0 disassembled and compared, port by port, with CompuPro's own `ROM1.ASM` (PROM1A1, Aug 1984): the same ports, DMA address and Read Data command; this revision adds `LXI SP,8000` and has Specify `03 8F 24` where `ROM1.ASM` has `03 DF 12`. **And it boots**: `pascal100 disk1a` loads CompuPro CP/M 2.2 (`64K CP/M 2.2LD`) off an 8″ disk through routine 0 and reaches `A>`. |
| `pcvideo` | CompuPro PC Video character generator (2764, U6) | **8192 B** (`0000`–`1FFF`): the 9x14 monochrome face at `0000`–`0FFF`, the 8x8 color face at `1000`–`1FFF`, 16 bytes a character | **`6555EFCD`** | `roms/PCVIDEO/` — `PCVIDEO.BIN`, made by `source/make_prom.py` from **Modern DOS** 20190101.02 (Jayvee Enaguas, **CC0 1.0**; the 9x14 and 8x8 faces, downloaded 2026-11-20 from `github.com/susam/pcface`, `src/font/moderndos/`). **Not a dump of CompuPro's PROM**: the layout is CompuPro's, the glyphs are a public-domain font. | The layout against the *PC Video Technical Manual* (A493) pp. 42 and 61 (A12 = MXSEL, A11–A4 = the character, A3–A0 = RA3–RA0). Every one of the 256 characters is present in both faces, and the 9th column of the 9x14 face is blank, or a copy of the 8th for `C0`–`DF`, exactly as the board makes it (p. 62). |

## Licensing

Most of these parts are from companies that no longer exist, and vintage ROM images circulate freely — but "freely circulating" is not the same as "licensed to redistribute," and **embedding a ROM in the binary is redistribution** in a way that asking a user to supply a file is not.

This is the maintainer's call, not the simulator's, and it is recorded here so it is a decision rather than an accident. (In altairsim it was Patrick Linstruth's call, and the paragraphs below began there.) If any ROM turns out to be one we should not ship, the fallback costs nothing: it stays a path (`mount = "roms/x.bin"`), the user supplies it once, and `docs/roms.md` says where to get it.

`tarbell-sd` is a **Tarbell Electronics** part — the 32-byte boot PROM from the #1011/#2022 floppy interfaces. Tarbell Electronics is long defunct, the bootstrap is 32 bytes reproduced from the interface manual's own listing, and it ships on the same footing as the other parts here — reversible to a path if it should not have been.

`ddboot` is a **Jade Computer Products** part, the host boot PROM for the Double D controller. Jade is long defunct. The source is on the Jade CP/M 2.2 master disk, which is in the maintainer's collection. It ships on the same footing as the Tarbell part above — reversible to a path if it should not have been. `ddboot-2sio` is the same PROM with the console patch that Jade's header asks the owner to make, here for an 88-2SIO.

`disk1a` is a **CompuPro** part (Godbout Electronics), the boot EPROM of the Disk 1A floppy controller. CompuPro is long defunct, and the image circulates freely in the vintage archives (Herb Johnson's retrotechnology.com ROM collection).

`disk1` is a **CompuPro** part, the boot EPROM of the Disk 1 floppy controller. The image circulates freely in the vintage archives (deramp.com).

`pcvideo` is **not a vendor part.** It has the layout of the CompuPro PC Video's character PROM and the glyphs of Modern DOS, a font released under CC0 1.0, so it has no redistribution question. The IBM character ROM and the clone makers' copies remain their owners' copyright: they are not embedded, and a user who has a dump of a real U6 gives it as a path.

**Each ROM directory carries its manual and a `README.md`.** The vendor manual PDFs are retained in `roms/<NAME>/` as downloaded, and each `README.md` is distilled from that manual (plus the ROM's own `.ASM` header) — what the ROM is, its load address and entry points, its I/O and RAM needs, and the `mount =` line to use it. These are the one exception to the repo's "no vendor PDFs" rule (`.gitignore`): they are small, they are the primary-source manuals for parts we embed, and keeping them beside the image is the same provenance discipline the table above exists to enforce.

## How it works

At build time, CMake turns each file in `roms/` into a byte array in a generated translation unit, and a registry maps `builtin:<name>` → `{span<const uint8_t>, crc32, description}`. Consequences worth stating:

- **No filesystem access at runtime** for a built-in. It is `.rodata`. Nothing to find, nothing to permission, nothing to ship alongside.
- **The board does not care.** A region takes a `span<const uint8_t>`; whether that came from `.rodata` or a file the host service read is not its business (§7). The `builtin:` scheme is resolved by the config loader, above the board.
- **`CONFIG SAVE` round-trips the name, not the bytes** — `mount = "builtin:disk1"` in, `mount = "builtin:disk1"` out.
- **A CRC test per ROM.** Cheap, and it turns "someone's editor mangled a binary" from a mystery into a build failure.
- **The `SHOW ROMS` description is one hand-written line per ROM**, in `roms/<NAME>/DESC`. Every built-in has one, so the column is uniform and each is visible and editable in the tree — not derived by a heuristic no one can see. Editing a `DESC` re-runs the embed; a ROM with no `DESC` still builds (blank column) but warns.

## References

- `DESIGN.md` §0.1 (where hardware facts come from), §7 (host services), §10.2 (`LOAD … ROM` — how you get bytes into a ROM at runtime).
- `docs/boards/s100-memory.md` — the board that holds them.
