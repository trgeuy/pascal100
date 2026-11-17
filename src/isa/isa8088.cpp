// The 8088 instruction set -- a stateless disassembler, as the 8080, 8085 and Z80 ones are.
//
// THE DECODE is written from Intel's encoding tables: "iAPX 86/10, 88/10 Instruction Set
// Summary" in the iAPX 88 Book (Intel, July 1981, pp. 61-62; Documentation/CompuPro/New
// Manuals/Intel iAPX Book.pdf). Nothing here comes from another simulator (DESIGN.md 0.1).
//
// THE TEXT is DDT-86's. DRI's debugger is what a CP/M-86 or MP/M 8-16 user listed code with,
// so this prints what DDT86 1.1 prints (its `L` listing, checked byte for byte on the
// CP/M-86 example disk -- tests/test_8088_isa.cpp): ASM-86 mnemonics (JMPS, JMPF, CALLF,
// RETF), a displacement IN FRONT of the brackets as raw hex (05[BX+SI], FC[BP]), BYTE/WORD
// with no PTR and only where no register gives the size, and each prefix as a field of its
// own (ES:    MOV    AL,[BX]).
//
// THE UNDOCUMENTED FORMS. DDT86 does not know them: it prints `??=  60` and carries on one
// byte later. The 8088 runs every one of them, so this decodes each to what the chip does,
// with its real length, and marks it as the 8080 decoder marks its holes: `??= 64  *JZ ...`.
// What each one does is from the SingleStepTests 8088 suite, recorded from a real 8088.
//
// ADDRESSES ARE OFFSETS IN ONE SEGMENT. The caller passes IP (an offset in CS) and a peek that
// adds the segment base, so a branch target is an offset in the same segment -- as DDT86 and
// an ASM-86 listing print it, and the value an 8086 .SYM holds.

#include "isa/isa.h"

#include <cstdio>
#include <string>

namespace altair {
namespace {

// An operand in the requested base -- the 8080 decoder's fmtNum rule, kept local as the
// 8085's is: `digits` is the hex width (2 or 4); octal is three digits for a byte and SPLIT
// octal (hi lo) for a word.
std::string fmtNum(unsigned v, int digits, int base) {
    char b[16];
    if (base == 8) {
        if (digits <= 2)
            std::snprintf(b, sizeof b, "%03o", v & 0xFF);
        else
            std::snprintf(b, sizeof b, "%03o %03o", (v >> 8) & 0xFF, v & 0xFF);
        return b;
    }
    if (digits <= 2)
        std::snprintf(b, sizeof b, "%02X", v & 0xFF);
    else
        std::snprintf(b, sizeof b, "%04X", v & 0xFFFF);
    return b;
}

const char* const kReg8[8] = {"AL", "CL", "DL", "BL", "AH", "CH", "DH", "BH"};
const char* const kReg16[8] = {"AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI"};
const char* const kSeg[4] = {"ES", "CS", "SS", "DS"};
// The r/m base registers, by r/m (iAPX 88 Book p. 62, the EA table).
const char* const kEa[8] = {"BX+SI", "BX+DI", "BP+SI", "BP+DI", "SI", "DI", "BP", "BX"};
// 70-7F (and the 60-6F aliases), in DDT86's spelling.
const char* const kJcc[16] = {"JO", "JNO", "JB",  "JNB", "JZ", "JNZ", "JBE", "JA",
                              "JS", "JNS", "JP",  "JNP", "JL", "JNL", "JLE", "JG"};
const char* const kAlu[8] = {"ADD", "OR", "ADC", "SBB", "AND", "SUB", "XOR", "CMP"};
const char* const kShift[8] = {"ROL", "ROR", "RCL", "RCR", "SHL", "SHR", "SETMO", "SAR"};

// The bytes of one instruction, read through the peek as the decode needs them.
struct Reader {
    const PeekFn& peek;
    uint16_t addr;
    int n = 0;  // bytes consumed so far
    uint8_t b() { return peek((uint16_t)(addr + n++)); }
    uint16_t w() {
        uint16_t lo = b();
        return (uint16_t)(lo | (b() << 8));
    }
};

// A decoded ModRM byte and the operand it names.
struct ModRm {
    int mod = 0, reg = 0, rm = 0;
    bool isMem() const { return mod != 3; }
    uint16_t disp = 0;   // mod 1: the byte (raw), mod 2: the word, mod 0 rm 6: the address
};

ModRm readModRm(Reader& r) {
    ModRm m;
    const uint8_t v = r.b();
    m.mod = v >> 6;
    m.reg = (v >> 3) & 7;
    m.rm = v & 7;
    if (m.mod == 1) m.disp = r.b();
    else if (m.mod == 2 || (m.mod == 0 && m.rm == 6)) m.disp = r.w();
    return m;
}

class Isa8088 final : public Disassembler {
public:
    const char* name() const override { return "8088"; }

    Insn at(uint16_t addr, const PeekFn& peek, int base) const override {
        Reader r{peek, addr};
        Insn out;
        bool undoc = false;
        int undocOp = -1;  // the byte the ??= marking names

        auto N2 = [&](unsigned v) { return fmtNum(v, 2, base); };
        auto N4 = [&](unsigned v) { return fmtNum(v, 4, base); };
        auto mark = [&](uint8_t op) {
            if (!undoc) undocOp = op;
            undoc = true;
        };

        // PREFIXES, each a field of its own, as DDT86 shows them. F1 is the 8088's
        // undocumented second LOCK. A run longer than the 16 bytes an instruction may
        // occupy here is listed one prefix at a time.
        std::string prefix;
        int prefixes = 0;
        uint8_t op = 0;
        for (;;) {
            op = r.b();
            const char* p = nullptr;
            switch (op) {
            case 0x26: p = "ES:"; break;
            case 0x2E: p = "CS:"; break;
            case 0x36: p = "SS:"; break;
            case 0x3E: p = "DS:"; break;
            case 0xF0: p = "LOCK"; break;
            case 0xF1: p = "LOCK"; mark(0xF1); break;
            case 0xF2: p = "REPNE"; break;
            case 0xF3: p = "REP"; break;
            default: break;
            }
            if (!p) break;
            if (++prefixes == 15) {  // all prefixes: show the first as an instruction alone
                r.n = 1;
                out.len = 1;
                out.text = prefixOnly(peek(addr), base);
                out.undocumented = peek(addr) == 0xF1;
                return out;
            }
            char f[16];
            std::snprintf(f, sizeof f, "%-6s ", p);
            prefix += f;
        }

        std::string m, ops;
        // The operand a symbol can name: a branch target, a direct address, a word immediate.
        auto name16 = [&](uint16_t v) {
            if (out.operandBits == 0) {
                out.operand = v;
                out.operandBits = 16;
            }
        };
        auto memText = [&](const ModRm& mr) {
            if (mr.mod == 0 && mr.rm == 6) {
                name16(mr.disp);
                return "[" + N4(mr.disp) + "]";
            }
            std::string d;
            if (mr.mod == 1) d = N2(mr.disp);
            if (mr.mod == 2) d = N4(mr.disp);
            return d + "[" + kEa[mr.rm] + "]";
        };
        // The r/m operand: a register of the width, or memory.
        auto ea = [&](const ModRm& mr, bool word) {
            if (!mr.isMem()) return std::string(word ? kReg16[mr.rm] : kReg8[mr.rm]);
            return memText(mr);
        };
        // The r/m operand where nothing else gives its size: BYTE/WORD on memory.
        auto eaSized = [&](const ModRm& mr, bool word) {
            if (!mr.isMem()) return ea(mr, word);
            return std::string(word ? "WORD " : "BYTE ") + memText(mr);
        };
        auto rel = [&](int disp) {
            const uint16_t t = (uint16_t)(addr + r.n + disp);
            name16(t);
            return N4(t);
        };

        const bool w = op & 1;
        if (op < 0x40 && (op & 7) < 6) {
            // The eight ALU operations: 00-05, 08-0D, ... 38-3D (p. 61).
            m = kAlu[op >> 3];
            switch (op & 7) {
            case 0: case 1: { ModRm mr = readModRm(r); ops = ea(mr, w) + "," + (w ? kReg16 : kReg8)[mr.reg]; break; }
            case 2: case 3: { ModRm mr = readModRm(r); ops = std::string((w ? kReg16 : kReg8)[mr.reg]) + "," + ea(mr, w); break; }
            case 4: ops = "AL," + N2(r.b()); break;
            case 5: { uint16_t v = r.w(); name16(v); ops = "AX," + N4(v); break; }
            }
        } else if (op < 0x40) {
            switch (op) {
            case 0x06: case 0x0E: case 0x16: case 0x1E: m = "PUSH"; ops = kSeg[op >> 3]; break;
            case 0x07: case 0x17: case 0x1F: m = "POP"; ops = kSeg[op >> 3]; break;
            case 0x0F: m = "POP"; ops = "CS"; mark(op); break;  // the 8086/8088's POP CS
            case 0x27: m = "DAA"; break;
            case 0x2F: m = "DAS"; break;
            case 0x37: m = "AAA"; break;
            case 0x3F: m = "AAS"; break;
            }
        } else if (op < 0x50) {
            m = op < 0x48 ? "INC" : "DEC";
            ops = kReg16[op & 7];
        } else if (op < 0x60) {
            m = op < 0x58 ? "PUSH" : "POP";
            ops = kReg16[op & 7];
        } else if (op < 0x80) {
            if (op < 0x70) mark(op);  // 60-6F: the 70-7F jumps again on the 8086/8088
            m = kJcc[op & 15];
            ops = rel((int8_t)r.b());
        } else {
            switch (op) {
            case 0x80: case 0x81: case 0x82: case 0x83: {
                // Immediate to register/memory. 83 sign-extends its byte, which Intel
                // allows only for ADD, ADC, SBB, SUB and CMP (s = 1); OR, AND and XOR are
                // 1000000w (p. 61), so 83 /1, /4 and /6 are undocumented. 82 repeats 80.
                ModRm mr = readModRm(r);
                if (op == 0x82 || (op == 0x83 && (mr.reg == 1 || mr.reg == 4 || mr.reg == 6)))
                    mark(op);
                m = kAlu[mr.reg];
                uint16_t v = op == 0x81 ? r.w() : r.b();
                if (op == 0x83) v = (uint16_t)(int16_t)(int8_t)v;
                if (op == 0x81 || op == 0x83) name16(v);
                ops = eaSized(mr, w) + "," + (w ? N4(v) : N2(v));
                break;
            }
            case 0x84: case 0x85: { ModRm mr = readModRm(r); m = "TEST"; ops = ea(mr, w) + "," + (w ? kReg16 : kReg8)[mr.reg]; break; }
            case 0x86: case 0x87: { ModRm mr = readModRm(r); m = "XCHG"; ops = std::string((w ? kReg16 : kReg8)[mr.reg]) + "," + ea(mr, w); break; }
            case 0x88: case 0x89: { ModRm mr = readModRm(r); m = "MOV"; ops = ea(mr, w) + "," + (w ? kReg16 : kReg8)[mr.reg]; break; }
            case 0x8A: case 0x8B: { ModRm mr = readModRm(r); m = "MOV"; ops = std::string((w ? kReg16 : kReg8)[mr.reg]) + "," + ea(mr, w); break; }
            case 0x8C: case 0x8E: {
                // Segment register moves; reg 4-7 repeat ES-DS on the 8086/8088 (undoc.)
                ModRm mr = readModRm(r);
                if (mr.reg > 3) mark(op);
                m = "MOV";
                const std::string s = kSeg[mr.reg & 3];
                ops = op == 0x8C ? ea(mr, true) + "," + s : s + "," + ea(mr, true);
                break;
            }
            case 0x8D: case 0xC4: case 0xC5: {
                ModRm mr = readModRm(r);
                if (!mr.isMem()) mark(op);  // a register "address": undocumented
                m = op == 0x8D ? "LEA" : op == 0xC4 ? "LES" : "LDS";
                ops = std::string(kReg16[mr.reg]) + "," + ea(mr, true);
                break;
            }
            case 0x8F: {
                ModRm mr = readModRm(r);
                if (mr.reg != 0) mark(op);  // 8F /1-/7 pop as /0 does
                m = "POP";
                ops = ea(mr, true);
                break;
            }
            case 0x90: m = "NOP"; break;
            case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
                m = "XCHG"; ops = std::string("AX,") + kReg16[op & 7]; break;
            case 0x98: m = "CBW"; break;
            case 0x99: m = "CWD"; break;
            case 0x9A: case 0xEA: {
                const uint16_t off = r.w(), seg = r.w();
                m = op == 0x9A ? "CALLF" : "JMPF";
                ops = N4(seg) + ":" + N4(off);
                break;
            }
            case 0x9B: m = "WAIT"; break;
            case 0x9C: m = "PUSHF"; break;
            case 0x9D: m = "POPF"; break;
            case 0x9E: m = "SAHF"; break;
            case 0x9F: m = "LAHF"; break;
            case 0xA0: case 0xA1: { uint16_t a = r.w(); name16(a); m = "MOV"; ops = std::string(w ? "AX" : "AL") + ",[" + N4(a) + "]"; break; }
            case 0xA2: case 0xA3: { uint16_t a = r.w(); name16(a); m = "MOV"; ops = "[" + N4(a) + "]," + (w ? "AX" : "AL"); break; }
            case 0xA4: m = "MOVSB"; break;
            case 0xA5: m = "MOVSW"; break;
            case 0xA6: m = "CMPSB"; break;
            case 0xA7: m = "CMPSW"; break;
            case 0xA8: m = "TEST"; ops = "AL," + N2(r.b()); break;
            case 0xA9: { uint16_t v = r.w(); name16(v); m = "TEST"; ops = "AX," + N4(v); break; }
            case 0xAA: m = "STOSB"; break;
            case 0xAB: m = "STOSW"; break;
            case 0xAC: m = "LODSB"; break;
            case 0xAD: m = "LODSW"; break;
            case 0xAE: m = "SCASB"; break;
            case 0xAF: m = "SCASW"; break;
            case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7:
                m = "MOV"; ops = std::string(kReg8[op & 7]) + "," + N2(r.b()); break;
            case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF: {
                uint16_t v = r.w(); name16(v);
                m = "MOV"; ops = std::string(kReg16[op & 7]) + "," + N4(v); break;
            }
            case 0xC0: case 0xC2: case 0xC8: case 0xCA: {
                // RET/RETF n; C0 and C8 repeat C2 and CA on the 8086/8088 (undoc.)
                if (op == 0xC0 || op == 0xC8) mark(op);
                m = op < 0xC8 ? "RET" : "RETF";
                ops = N4(r.w());
                break;
            }
            case 0xC1: case 0xC3: case 0xC9: case 0xCB:
                if (op == 0xC1 || op == 0xC9) mark(op);
                m = op < 0xC8 ? "RET" : "RETF";
                break;
            case 0xC6: case 0xC7: {
                ModRm mr = readModRm(r);
                if (mr.reg != 0) mark(op);  // C6/C7 /1-/7 move as /0 does
                m = "MOV";
                uint16_t v = w ? r.w() : r.b();
                if (w) name16(v);
                ops = eaSized(mr, w) + "," + (w ? N4(v) : N2(v));
                break;
            }
            case 0xCC: m = "INT"; ops = "3"; break;
            case 0xCD: m = "INT"; ops = N2(r.b()); break;
            case 0xCE: m = "INTO"; break;
            case 0xCF: m = "IRET"; break;
            case 0xD0: case 0xD1: case 0xD2: case 0xD3: {
                // Shifts and rotates by 1 or by CL; /6 is the 8088's undocumented SETMO
                // (SETMOC by CL), which sets the operand to all ones.
                ModRm mr = readModRm(r);
                const bool byCl = op >= 0xD2;
                if (mr.reg == 6) mark(op);
                m = mr.reg == 6 && byCl ? "SETMOC" : kShift[mr.reg];
                ops = eaSized(mr, w) + (byCl ? ",CL" : ",1");
                break;
            }
            case 0xD4: case 0xD5: {
                // AAM/AAD take a base byte; Intel documents only 0A. The chip uses any.
                const uint8_t b = r.b();
                m = op == 0xD4 ? "AAM" : "AAD";
                if (b != 0x0A) {
                    mark(op);
                    ops = N2(b);
                }
                break;
            }
            case 0xD6: m = "SALC"; mark(op); break;  // AL = CF ? FF : 00
            case 0xD7: m = "XLAT"; break;
            case 0xD8: case 0xD9: case 0xDA: case 0xDB: case 0xDC: case 0xDD: case 0xDE: case 0xDF: {
                // ESC n,r/m: n is the opcode's low three bits and the ModRM reg field. The
                // register form takes its width from the opcode's low bit, as DDT86 shows.
                ModRm mr = readModRm(r);
                m = "ESC";
                ops = N2(((op & 7u) << 3) | (unsigned)mr.reg) + "," + ea(mr, w);
                break;
            }
            case 0xE0: m = "LOOPNE"; ops = rel((int8_t)r.b()); break;
            case 0xE1: m = "LOOPE"; ops = rel((int8_t)r.b()); break;
            case 0xE2: m = "LOOP"; ops = rel((int8_t)r.b()); break;
            case 0xE3: m = "JCXZ"; ops = rel((int8_t)r.b()); break;
            case 0xE4: case 0xE5: m = "IN"; ops = std::string(w ? "AX," : "AL,") + N2(r.b()); break;
            case 0xE6: case 0xE7: m = "OUT"; ops = N2(r.b()) + (w ? ",AX" : ",AL"); break;
            case 0xE8: m = "CALL"; ops = rel((int16_t)r.w()); break;
            case 0xE9: m = "JMP"; ops = rel((int16_t)r.w()); break;
            case 0xEB: m = "JMPS"; ops = rel((int8_t)r.b()); break;
            case 0xEC: case 0xED: m = "IN"; ops = w ? "AX,DX" : "AL,DX"; break;
            case 0xEE: case 0xEF: m = "OUT"; ops = w ? "DX,AX" : "DX,AL"; break;
            case 0xF4: m = "HLT"; break;
            case 0xF5: m = "CMC"; break;
            case 0xF6: case 0xF7: {
                static const char* const g3[8] = {"TEST", "TEST", "NOT", "NEG", "MUL", "IMUL", "DIV", "IDIV"};
                ModRm mr = readModRm(r);
                m = g3[mr.reg];
                ops = eaSized(mr, w);
                if (mr.reg < 2) {  // TEST takes an immediate; /1 repeats /0 (undoc.)
                    if (mr.reg == 1) mark(op);
                    uint16_t v = w ? r.w() : r.b();
                    if (w) name16(v);
                    ops += "," + (w ? N4(v) : N2(v));
                }
                break;
            }
            case 0xF8: m = "CLC"; break;
            case 0xF9: m = "STC"; break;
            case 0xFA: m = "CLI"; break;
            case 0xFB: m = "STI"; break;
            case 0xFC: m = "CLD"; break;
            case 0xFD: m = "STD"; break;
            case 0xFE: case 0xFF: {
                // INC/DEC, and on FF the indirect CALL/CALLF/JMP/JMPF and PUSH. FE /2-/7
                // run those on a byte, FF /7 pushes as /6, and a far one through a register
                // has no pointer to read: all undocumented.
                static const char* const g5[8] = {"INC", "DEC", "CALL", "CALLF", "JMP", "JMPF", "PUSH", "PUSH"};
                ModRm mr = readModRm(r);
                m = g5[mr.reg];
                if ((op == 0xFE && mr.reg > 1) || mr.reg == 7 ||
                    ((mr.reg == 3 || mr.reg == 5) && !mr.isMem()))
                    mark(op);
                ops = mr.reg < 2 ? eaSized(mr, w) : ea(mr, w);
                break;
            }
            }
        }

        out.len = (uint8_t)r.n;
        char f[96];
        if (ops.empty())
            std::snprintf(f, sizeof f, "%s", m.c_str());
        else
            std::snprintf(f, sizeof f, "%-6s %s", m.c_str(), ops.c_str());
        std::string text = prefix + f;
        while (!text.empty() && text.back() == ' ') text.pop_back();
        out.undocumented = undoc;
        out.text = undoc ? "?\?= " + fmtNum((unsigned)undocOp, 2, base) + "  *" + text : text;
        return out;
    }

private:
    static std::string prefixOnly(uint8_t p, int base) {
        switch (p) {
        case 0x26: return "ES:";
        case 0x2E: return "CS:";
        case 0x36: return "SS:";
        case 0x3E: return "DS:";
        case 0xF0: return "LOCK";
        case 0xF1: return "?\?= " + fmtNum(0xF1, 2, base) + "  *LOCK";
        case 0xF2: return "REPNE";
        default:   return "REP";
        }
    }
};

}  // namespace

// Registered in isa8080.cpp, with the others.
const Disassembler* isa8088Disassembler() {
    static const Isa8088 d;
    return &d;
}

}  // namespace altair
