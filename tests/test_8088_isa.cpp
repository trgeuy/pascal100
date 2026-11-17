#include "test.h"

#include "isa/isa.h"

#include <string>
#include <vector>

using namespace altair;

// THE 8088 DISASSEMBLER (isa/isa8088.cpp): CLK's x86 decoder, our spelling. The bytes are
// from CompuPro's MP/M 8-16 Shell (CS 1759), checked against GNU objdump -m i8086, plus the
// forms CLK's own to_string gets wrong: a zero immediate, a word immediate, [SI] with no
// displacement, a backward branch, an indirect far jump.

void test_8088_isa() {
    SECTION("the 8088 instruction set -- CLK decodes, offsets in one segment");

    const Disassembler* d = disassemblerFor("8088");
    CHECK(d != nullptr, "we speak 8088");
    if (!d) return;
    CHECK(std::string(d->name()) == "8088", "and it says so");
    bool listed = false;
    for (const std::string& s : instructionSets()) listed = listed || s == "8088";
    CHECK(listed, "instructionSets() names 8088");

    // Decode `bytes` placed at offset `at`.
    auto dis = [&](uint16_t at, std::vector<uint8_t> bytes) {
        auto p = [&](uint16_t a) -> uint8_t {
            uint16_t k = (uint16_t)(a - at);
            return k < bytes.size() ? bytes[k] : (uint8_t)0x90;
        };
        return d->at(at, p);
    };
    auto is = [&](uint16_t at, std::vector<uint8_t> b, const char* want, int len) {
        Insn in = dis(at, b);
        CHECK(in.text == want && in.len == len,
              (std::string(want) + " -- got '" + in.text + "' len " + std::to_string(in.len)).c_str());
        return in;
    };

    is(0x019F, {0xBB, 0x2B, 0x02}, "MOV    BX,022B", 3);         // CLK alone: "2Bh"
    is(0x01A7, {0x3C, 0x00}, "CMP    AL,00", 2);                  // CLK alone: nothing
    is(0x01A5, {0x8A, 0x04}, "MOV    AL,[SI]", 2);                // CLK alone: [ds:si+0h]
    Insn jz = is(0x01A9, {0x74, 0x18}, "JZ     01C3", 2);         // the TARGET, not +18
    CHECK(jz.operandBits == 16 && jz.operand == 0x01C3, "a branch target is the operand a symbol names");
    is(0x01FF, {0xEB, 0xF2}, "JMP    01F3", 2);                   // backward: CLK alone FFFFFFF4h
    is(0x0207, {0x80, 0x3E, 0x3F, 0x02, 0xFF}, "CMP    BYTE PTR [023F],FF", 5);
    is(0x0061, {0x26, 0x8A, 0x36, 0x47, 0x00}, "MOV    DH,ES:[0047]", 5);
    is(0x00AF, {0xF3, 0xA5}, "REP MOVSW", 2);
    is(0x0F8A, {0x2E, 0xFF, 0x2E, 0x6F, 0x01}, "JMP    FAR CS:[016F]", 5);
    is(0x0000, {0xEA, 0x00, 0x04, 0x00, 0x00}, "JMP    0000:0400", 5);
    is(0x0BC1, {0xCD, 0xE0}, "INT    E0", 2);                     // the MP/M-86 entry
    is(0x08CB, {0xD0, 0xC8}, "ROR    AL,1", 2);
    is(0x0EC4, {0xEE}, "OUT    DX,AL", 1);
    is(0x0000, {0xE4, 0x5D}, "IN     AL,5D", 2);
    is(0x0000, {0x8B, 0x46, 0xFC}, "MOV    AX,[BP-04]", 3);

    // THE UNDOCUMENTED FORMS run on real silicon and are marked as the 8080 marks its own:
    // `??= op  *...`, with `undocumented` set. 60-6F are the 70-7F jumps again (objdump
    // -m i8086 calls 64 an FS prefix); C0 is RET n again; D6 is SALC.
    Insn alias = is(0x0100, {0x64, 0x02}, "?\?= 64  *JZ     0104", 2);
    CHECK(alias.undocumented && alias.operandBits == 16 && alias.operand == 0x0104,
          "an aliased jump is undocumented and still names its target");
    CHECK(is(0x10E2, {0xC0, 0x13, 0x5D}, "?\?= C0  *RET    5D13", 3).undocumented, "C0 = RET n");
    CHECK(is(0x0000, {0xD6}, "?\?= D6  *SALC", 1).undocumented, "D6 = SALC");
    CHECK(!dis(0x01A9, {0x74, 0x18}).undocumented, "a documented jump is not marked");

    // ESC names the coprocessor instruction: the low three opcode bits and ModRM reg.
    is(0x0EB8, {0xDC, 0x01}, "ESC    20,[BX+DI]", 2);
    is(0x0EB8, {0xD9, 0xC0}, "ESC    08,AX", 2);

    // OCTAL, when the console is set to it: a byte in three digits, a word split hi/lo.
    auto oct = [&](uint16_t at, std::vector<uint8_t> bytes) {
        auto p = [&](uint16_t a) -> uint8_t {
            uint16_t k = (uint16_t)(a - at);
            return k < bytes.size() ? bytes[k] : (uint8_t)0x90;
        };
        return d->at(at, p, 8).text;
    };
    CHECK(oct(0x019F, {0xBB, 0x2B, 0x02}) == "MOV    BX,002 053", "a word in split octal");
    CHECK(oct(0x01A9, {0x74, 0x18}) == "JZ     001 303", "a branch target in split octal");
    CHECK(oct(0x0000, {0x3C, 0x3A}) == "CMP    AL,072", "a byte in three octal digits");
}
