// The 8088 instruction set: CLK decodes, this file spells.
//
// Clock Signal's x86 Decoder already decodes every 8088 instruction the core executes, so the
// DECODE is proven by every program that runs. CLK's own to_string() is not fit for a listing
// (its header calls the decoder experimental): it drops a zero immediate, truncates a word
// immediate when no length is given, prints [ds:si+0h] for [SI], and prints a relative branch
// as a displacement, not a target. So the text is made here, in the monitor's style -- Intel
// operand order, upper case, hex with no suffix, as DOS DEBUG and SID-86 list:
//
//     MOV    BX,022B     JZ     01C3     MOV    AL,ES:[SI]     INC    BYTE PTR [BX+04]
//
// ADDRESSES ARE OFFSETS IN ONE SEGMENT. The Disassembler interface is 16-bit; for the 8088 the
// caller passes IP (an offset in CS) and a peek that adds the segment base. A near branch
// target is then an offset in the same segment, which is how an 8086 listing prints it, and
// the value a .SYM or .PRN symbol for 8086 code holds.

#include "isa/isa.h"

#include "InstructionSets/x86/Decoder.hpp"
#include "InstructionSets/x86/Instruction.hpp"

#include <cctype>
#include <cstdio>

namespace altair {
namespace {

using namespace InstructionSet::x86;
using X86 = Instruction<InstructionType::Bits16>;

std::string up(std::string s) {
    for (char& ch : s) ch = (char)std::toupper((unsigned char)ch);
    return s;
}

// An operand in the requested base -- the same rule as the 8080 decoder's fmtNum, kept
// local as the 8085's is: `digits` is the hex width (2 for a byte, 4 for a word); octal is a
// byte as three digits and a word as SPLIT octal, hi then lo.
std::string fmtNum(unsigned v, int digits, int base) {
    char b[16];
    if (base == 8) {
        if (digits <= 2)
            std::snprintf(b, sizeof b, "%03o", v & 0xFF);
        else
            std::snprintf(b, sizeof b, "%03o %03o", (v >> 8) & 0xFF, v & 0xFF);
        return b;
    }
    std::snprintf(b, sizeof b, digits <= 2 ? "%02X" : "%04X", digits <= 2 ? v & 0xFF : v & 0xFFFF);
    return b;
}

std::string reg(Source s, DataSize size) { return up(to_string(s, size)); }

bool isMem(Source s) {
    return s == Source::DirectAddress || s == Source::Indirect || s == Source::IndirectNoBase;
}

// A memory operand: [BX+SI+12], [BP-04], [1234], with the segment override (if one was in
// the prefix bytes) in front, as ES:[SI].
std::string mem(const DataPointer& p, const X86& in, const std::string& seg, int base) {
    std::string t = seg + "[";
    const DataSize as = DataSize::Word;  // 16-bit addressing on the 8086/8088
    switch (p.source()) {
    case Source::DirectAddress:
        t += fmtNum(in.offset(), 4, base);
        break;
    case Source::Indirect:
        t += reg(p.base(), as);
        if (p.index() != Source::None) t += "+" + reg(p.index(), as);
        break;
    case Source::IndirectNoBase:
        t += reg(p.index(), as);
        break;
    default:
        break;
    }
    if (p.source() != Source::DirectAddress) {
        const int d = (int16_t)in.offset();
        const unsigned a = (unsigned)(d < 0 ? -d : d);
        if (d != 0) t += (d < 0 ? "-" : "+") + fmtNum(a, a < 0x100 ? 2 : 4, base);
    }
    return t + "]";
}

std::string operand(const DataPointer& p, const X86& in, DataSize size, const std::string& seg,
                    int base) {
    const Source s = p.source();
    if (s == Source::Immediate) return fmtNum(in.operand(), size == DataSize::Byte ? 2 : 4, base);
    if (isMem(s)) return mem(p, in, seg, base);
    return reg(s, size);
}

// THE UNDOCUMENTED 8086/8088 forms, by opcode (and ModRM reg field where that is what decides).
// Real silicon runs every one, and so does the core; the listing marks them, as the 8080's
// does (`??= op  *...`). From the 8086's decode: 0F is POP CS; 60-6F repeat the 70-7F jumps;
// C0/C1 repeat C2/C3 and C8/C9 repeat CA/CB; D6 is SALC; F1 repeats the LOCK prefix; 82
// repeats 80; D0-D3 /6 is SETMO; F6/F7 /1 repeat TEST; 8C/8E name segment registers 4-7 as
// 0-3; FE /2-/7 and 8F /1-/7 are the word forms run on a byte.
bool undocumented(const uint8_t* b, int prefixes, Operation op) {
    if (op == Operation::SALC || op == Operation::SETMO || op == Operation::SETMOC) return true;
    for (int i = 0; i < prefixes; ++i)
        if (b[i] == 0xF1) return true;
    const uint8_t o = b[prefixes];
    const int r = (b[prefixes + 1] >> 3) & 7;  // the ModRM reg field, where there is one
    if (o == 0x0F || (o >= 0x60 && o <= 0x6F) || o == 0xC0 || o == 0xC1 || o == 0xC8 ||
        o == 0xC9 || o == 0xD6 || o == 0x82)
        return true;
    if ((o == 0xF6 || o == 0xF7) && r == 1) return true;
    if ((o == 0x8C || o == 0x8E) && r >= 4) return true;
    if (o == 0xFE && r >= 2) return true;
    if (o == 0x8F && r != 0) return true;
    return false;
}

class Isa8088 final : public Disassembler {
public:
    const char* name() const override { return "8088"; }

    Insn at(uint16_t addr, const PeekFn& peek, int base) const override {
        uint8_t b[18] = {};
        for (int i = 0; i < 16; ++i) b[i] = peek((uint16_t)(addr + i));

        Decoder<Model::i8086> dec;
        const auto r = dec.decode(b, 16);
        Insn out;
        if (r.first <= 0 || r.second.operation() == Operation::Invalid) {
            // Nothing in the 8086 map decodes to nothing, so this is a run of prefixes longer
            // than the 16 bytes read, or a byte CLK does not know. Say what the byte is.
            out.text = "DB     " + fmtNum(b[0], 2, base);
            out.len = 1;
            return out;
        }
        const X86& in = r.second;
        out.len = (uint8_t)r.first;

        // Prefix bytes, read from the bytes themselves: CLK folds REP into the operation and
        // keeps only the EFFECTIVE segment, so an explicit override is not otherwise visible.
        std::string seg, lock;
        int prefixes = 0;
        for (; prefixes < r.first - 1; ++prefixes) {
            const uint8_t p = b[prefixes];
            if (p == 0x26) seg = "ES:";
            else if (p == 0x2E) seg = "CS:";
            else if (p == 0x36) seg = "SS:";
            else if (p == 0x3E) seg = "DS:";
            else if (p == 0xF0 || p == 0xF1) lock = "LOCK ";
            else if (p != 0xF2 && p != 0xF3) break;
        }

        const Operation op = in.operation();
        const DataSize size = in.operation_size();
        std::string m = up(to_string(op, size, Model::i8086));
        if (op == Operation::RETnear) m = "RET";
        if (op == Operation::SAL) m = "SHL";
        if (op == Operation::JMPfar) m = "JMP";
        if (op == Operation::CALLfar) m = "CALL";

        std::string args;
        const DataPointer dst = in.destination(), src = in.source();

        if (has_displacement(op)) {
            // A relative branch: print the TARGET, an offset in this segment.
            const uint16_t target = (uint16_t)(addr + r.first + in.displacement());
            args = fmtNum(target, 4, base);
            out.operand = target;
            out.operandBits = 16;
        } else if ((op == Operation::JMPfar || op == Operation::CALLfar) &&
                   dst.source() == Source::Immediate) {
            args = fmtNum(in.segment(), 4, base) + ":" + fmtNum(in.offset(), 4, base);
        } else if (op == Operation::JMPfar || op == Operation::CALLfar) {
            // Indirect far: the 32-bit pointer in memory (JMP FAR [BX]). CLK puts the operand
            // in the destination or the source; print whichever is there, with no PTR.
            const DataPointer p = dst.source() != Source::None ? dst : src;
            args = "FAR " + operand(p, in, DataSize::Word, seg, base);
        } else if (op == Operation::IN || op == Operation::OUT) {
            const bool isIn = op == Operation::IN;
            const DataPointer port = isIn ? src : dst, data = isIn ? dst : src;
            const std::string p =
                port.source() == Source::DirectAddress ? fmtNum(in.offset(), 2, base) : "DX";
            const std::string d = reg(data.source(), size);
            args = isIn ? d + "," + p : p + "," + d;
        } else if (op == Operation::ESC) {
            // ESC n,operand: n is the six bits that name the coprocessor instruction -- the
            // low three of the opcode and the ModRM reg field (8086 manual, ESC).
            const unsigned n = ((b[prefixes] & 7u) << 3) | ((b[prefixes + 1] >> 3) & 7u);
            // The ModRM operand: CLK carries it as whichever side is memory (a register form,
            // mod = 11, has none, and then the register is the one there is).
            const DataPointer p = isMem(src.source())   ? src
                                  : isMem(dst.source()) ? dst
                                  : dst.source() != Source::None ? dst : src;
            args = fmtNum(n, 2, base);
            if (p.source() != Source::None) args += "," + operand(p, in, size, seg, base);
        } else {
            const int n = max_displayed_operands(op);
            const bool first = dst.source() != Source::None &&
                               (n > 1 || (n > 0 && src.source() == Source::None));
            const bool second = n > 0 && src.source() != Source::None;
            // Shifts and rotates: CLK lists CL as the source, and an implicit 1 as an
            // immediate 1 (the 8088 has only the "by 1" and "by CL" forms).
            const bool shift = op == Operation::RCL || op == Operation::RCR ||
                               op == Operation::ROL || op == Operation::ROR ||
                               op == Operation::SAL || op == Operation::SAR ||
                               op == Operation::SHR || op == Operation::SETMO ||
                               op == Operation::SETMOC;
            std::string a1 = first ? operand(dst, in, size, seg, base) : "";
            std::string a2;
            if (second) {
                if (shift) a2 = src.source() == Source::eCX ? "CL" : "1";
                else a2 = operand(src, in, (op == Operation::LDS || op == Operation::LES)
                                                  ? DataSize::Word : size, seg, base);
            } else if (shift) {
                a2 = "1";
            }
            // BYTE PTR / WORD PTR when no register says the size: a memory operand with
            // an immediate or alone (INC [BX], MOV [BX],05).
            const bool regSized = (!isMem(dst.source()) && dst.source() != Source::Immediate &&
                                   dst.source() != Source::None) ||
                                  (!shift && !isMem(src.source()) &&
                                   src.source() != Source::Immediate &&
                                   src.source() != Source::None);
            const bool needPtr = !regSized && !mnemonic_implies_data_size(op);
            const std::string ptr = size == DataSize::Byte ? "BYTE PTR " : "WORD PTR ";
            if (needPtr && first && isMem(dst.source())) a1 = ptr + a1;
            else if (needPtr && !first && second && isMem(src.source())) a2 = ptr + a2;
            args = a1;
            if (!a1.empty() && !a2.empty()) args += ",";
            args += a2;
            // An immediate word that a symbol could name (MOV BX,022B -> MOV BX,MSG).
            if (src.source() == Source::Immediate && size == DataSize::Word) {
                out.operand = (uint16_t)in.operand();
                out.operandBits = 16;
            }
        }

        m = lock + m;
        char buf[96];
        std::snprintf(buf, sizeof buf, "%-6s %s", m.c_str(), args.c_str());
        std::string text = buf;
        while (!text.empty() && text.back() == ' ') text.pop_back();

        out.undocumented = undocumented(b, prefixes, op);
        out.text = out.undocumented ? "?\?= " + fmtNum(b[prefixes], 2, base) + "  *" + text : text;
        return out;
    }
};

}  // namespace

// Registered in isa8080.cpp, with the others.
const Disassembler* isa8088Disassembler() {
    static const Isa8088 d;
    return &d;
}

}  // namespace altair
