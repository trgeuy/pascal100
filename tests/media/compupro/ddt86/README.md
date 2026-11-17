# DDT86 listing: the reference for the 8088 disassembler's text

`ddt86-l.txt` is what DRI's DDT86 1.1 prints for the 8088 instructions in a test file. The
8088 disassembler (`src/isa/isa8088.cpp`) prints DDT86's text, and `tests/test_8088_isa.cpp`
checks every line of this file against it.

## How it was made

1. `python3 -I gen-test-bin.py` writes `TEST.BIN` (2,898 bytes, 1,138 instructions: every
   opcode, every ModRM addressing form, the prefixes and the undocumented 8088 forms) and
   `INDEX.TXT` (the offset and the bytes of each instruction).
2. In the `examples/cpm86` machine (CP/M-86 1.1, with `DDT86.CMD` on its disk): `R TEST.BIN`,
   `DDT86`, `RTEST.BIN` (DDT86 loaded it at `0394:0000`), `L394:0,B51`.
3. The listing was matched to `INDEX.TXT` by offset. A line is kept when DDT86 decoded at the
   start of an instruction: 1,011 lines.

The other 127 instructions are not here. DDT86 does not know the undocumented forms (it
prints `??=  60` and goes on at the next byte), so it lists them wrongly, and it is out of
step for the instruction after one. The unit test checks those forms by hand.

DDT86 is Digital Research's program. This file holds only what it printed for our test bytes.
