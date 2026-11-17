#include "test.h"

#include "isa/isa.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace altair;

// THE 8088 DISASSEMBLER (isa/isa8088.cpp): Intel's encoding tables, DDT86's text.
//
// The text is checked against DDT86 1.1 itself: tests/media/compupro/ddt86/ddt86-l.txt is its
// `L` listing of a test file with every opcode and ModRM form (README.md there says how it was
// made). DDT86 does not know the undocumented 8088 forms, so those are checked by hand below.
// The lengths were also checked once, outside this test, against all 3,007,000 SingleStepTests
// 8088 v2 tests (recorded from a real 8088): every length agreed with the chip.

namespace {

// Decode `bytes` placed at offset `at`; NOPs after them.
Insn dis(const Disassembler* d, uint16_t at, const std::vector<uint8_t>& bytes, int base = 16) {
    auto p = [&](uint16_t a) -> uint8_t {
        uint16_t k = (uint16_t)(a - at);
        return k < bytes.size() ? bytes[k] : (uint8_t)0x90;
    };
    return d->at(at, p, base);
}

}  // namespace

void test_8088_isa() {
    SECTION("the 8088 instruction set -- Intel's tables, DDT86's text, offsets in one segment");

    const Disassembler* d = disassemblerFor("8088");
    CHECK(d != nullptr, "we speak 8088");
    if (!d) return;
    CHECK(std::string(d->name()) == "8088", "and it says so");
    bool listed = false;
    for (const std::string& s : instructionSets()) listed = listed || s == "8088";
    CHECK(listed, "instructionSets() names 8088");

    // EVERY LINE DDT86 PRINTED: same text, and the next line where DDT86's next line starts.
    {
        std::ifstream f(testRom("compupro/ddt86/ddt86-l.txt"));
        CHECK(f.good(), "tests/media/compupro/ddt86/ddt86-l.txt is there");
        std::string line;
        int lines = 0, bad = 0;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();  // a CRLF checkout
            if (line.empty() || line[0] == '#') continue;
            std::istringstream s(line);
            std::string off, hex, text;
            std::getline(s, off, '\t');
            std::getline(s, hex, '\t');
            std::getline(s, text);
            std::vector<uint8_t> b;
            std::istringstream h(hex);
            std::string byte;
            while (h >> byte) b.push_back((uint8_t)std::strtoul(byte.c_str(), nullptr, 16));
            const uint16_t at = (uint16_t)std::strtoul(off.c_str(), nullptr, 16);
            Insn in = dis(d, at, b);
            ++lines;
            if (in.text != text || in.len != b.size()) {
                if (++bad <= 5)
                    std::printf("        %04X %s: got '%s' len %d, DDT86 '%s' len %zu\n", at,
                                hex.c_str(), in.text.c_str(), in.len, text.c_str(), b.size());
            }
        }
        CHECK(lines == 1011, "all 1011 DDT86 lines read");
        CHECK(bad == 0, "every DDT86 line decodes to DDT86's text and length");
    }

    auto is = [&](uint16_t at, std::vector<uint8_t> b, const char* want, int len) {
        Insn in = dis(d, at, b);
        CHECK(in.text == want && in.len == len,
              (std::string(want) + " -- got '" + in.text + "' len " + std::to_string(in.len)).c_str());
        return in;
    };

    // DDT86's style, from the CP/M-86 Programmer's Guide sample session (p. 75) as well.
    is(0x0008, {0x8A, 0x00}, "MOV    AL,[BX+SI]", 2);
    is(0x000D, {0x3A, 0x40, 0x01}, "CMP    AL,01[BX+SI]", 3);
    is(0x0017, {0xC6, 0x06, 0x08, 0x01, 0x01}, "MOV    BYTE [0108],01", 5);
    is(0x0000, {0x8B, 0x46, 0xFC}, "MOV    AX,FC[BP]", 3);  // the displacement as DDT86 shows it
    is(0x0000, {0x26, 0x8A, 0x07}, "ES:    MOV    AL,[BX]", 3);
    is(0x0000, {0xF3, 0x26, 0xA5}, "REP    ES:    MOVSW", 3);

    // THE OPERAND A SYMBOL NAMES: a branch target, a direct address, a word immediate.
    Insn jz = is(0x01A9, {0x74, 0x18}, "JZ     01C3", 2);
    CHECK(jz.operandBits == 16 && jz.operand == 0x01C3, "a branch target is the operand");
    is(0x01FF, {0xEB, 0xF2}, "JMPS   01F3", 2);
    Insn mv = is(0x019F, {0xBB, 0x2B, 0x02}, "MOV    BX,022B", 3);
    CHECK(mv.operandBits == 16 && mv.operand == 0x022B, "a word immediate is the operand");
    Insn dir = is(0x0000, {0xA0, 0x34, 0x12}, "MOV    AL,[1234]", 3);
    CHECK(dir.operandBits == 16 && dir.operand == 0x1234, "a direct address is the operand");
    CHECK(dis(d, 0, {0x04, 0x12}).operandBits == 0, "a byte immediate is not");

    // THE UNDOCUMENTED FORMS: decoded to what the 8088 does, with its length, and marked as
    // the 8080's holes are. DDT86 prints `??=` and goes on one byte later instead.
    auto undoc = [&](uint16_t at, std::vector<uint8_t> b, const char* want, int len) {
        Insn in = is(at, b, want, len);
        CHECK(in.undocumented, (std::string(want) + " is marked undocumented").c_str());
    };
    undoc(0x0100, {0x64, 0x02}, "?\?= 64  *JZ     0104", 2);  // 60-6F = 70-7F
    undoc(0x0000, {0x0F}, "?\?= 0F  *POP    CS", 1);
    undoc(0x0000, {0x82, 0x07, 0x12}, "?\?= 82  *ADD    BYTE [BX],12", 3);
    undoc(0x0000, {0x83, 0xCB, 0x12}, "?\?= 83  *OR     BX,0012", 3);  // no s=1 for OR/AND/XOR
    undoc(0x0000, {0x8C, 0xE0}, "?\?= 8C  *MOV    AX,ES", 2);       // segment reg 4 = ES
    undoc(0x0000, {0x8F, 0x0F}, "?\?= 8F  *POP    [BX]", 2);
    undoc(0x0000, {0xC0, 0x04, 0x00}, "?\?= C0  *RET    0004", 3);
    undoc(0x0000, {0xC9}, "?\?= C9  *RETF", 1);
    undoc(0x0000, {0xC6, 0x0F, 0x12}, "?\?= C6  *MOV    BYTE [BX],12", 3);
    undoc(0x0000, {0xD0, 0x37}, "?\?= D0  *SETMO  BYTE [BX],1", 2);
    undoc(0x0000, {0xD3, 0xF1}, "?\?= D3  *SETMOC CX,CL", 2);
    undoc(0x0000, {0xD4, 0x08}, "?\?= D4  *AAM    08", 2);
    undoc(0x0000, {0xD6}, "?\?= D6  *SALC", 1);
    undoc(0x0000, {0xF1, 0x87, 0x07}, "?\?= F1  *LOCK   XCHG   AX,[BX]", 3);
    undoc(0x0000, {0xF6, 0x0F, 0x12}, "?\?= F6  *TEST   BYTE [BX],12", 3);
    undoc(0x0000, {0xFE, 0x17}, "?\?= FE  *CALL   [BX]", 2);
    undoc(0x0000, {0xFF, 0x3F}, "?\?= FF  *PUSH   [BX]", 2);
    CHECK(!dis(d, 0, {0x74, 0x18}).undocumented, "a documented jump is not marked");
    CHECK(!dis(d, 0, {0xD4, 0x0A}).undocumented, "AAM with base 0A is documented");

    // OCTAL, when the console is set to it: a byte in three digits, a word split hi/lo.
    CHECK(dis(d, 0x019F, {0xBB, 0x2B, 0x02}, 8).text == "MOV    BX,002 053", "a word in split octal");
    CHECK(dis(d, 0x01A9, {0x74, 0x18}, 8).text == "JZ     001 303", "a branch target in split octal");
    CHECK(dis(d, 0x0000, {0x3C, 0x3A}, 8).text == "CMP    AL,072", "a byte in three octal digits");
}
