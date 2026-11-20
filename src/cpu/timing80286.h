#pragma once
//
// 80286 INSTRUCTION TIMING, real mode, in processor clocks, for cpu80286.cpp.
//
// SOURCE: Intel, "80286 and 80287 Programmer's Reference Manual", 210498-005 (1987),
// chapter 8 (the instruction dictionary: one clock table per instruction) and its rules
// in section 8.2:
//
//   * The unlabelled count is for a register operand, "mem=" for a memory operand.
//   * +1 clock for the base + index + displacement form of address.
//   * +2 clocks for each 16-bit memory operand at an odd address. The core counts the word
//     transfers that took two bus cycles (an odd address, or a board with no SIXTN*).
//   * A taken jump, a CALL, a RET, an INT and the like: +1 clock for each byte of the next
//     instruction executed. x86_impl.h adds this when it decodes that instruction.
//   * Shifts and rotates by a count: +1 clock for each bit shifted (the count is masked to
//     5 bits on the 80286).
//   * Wait states: +1 clock for each wait state (the card adds them, Bus::addWaitStates).
//
// The counts assume that the prefetch queue is ahead of the instruction. Intel: real code
// "will average 5-10% more than the calculated clock count". The queue is not modelled.
// CHECKED against the SingleStepTests 80286 cycle counts (a real 80C286; tests/cpu/80286/).
// The suite starts each test after a jump (an empty queue) and ends it with HLT, which adds
// about 11 clocks to every test. With that overhead, 97% of 32,600 tests (100 per form) are
// within +-2 clocks. Three changes came from the suite, where the book has no count or the
// chip disagrees: ESC with no 80287, REP STOSW to an odd address, and the C0/C1 count.
// Left as the book has them: far CALL/JMP (the chip is about 3 clocks slower) and word
// IN/OUT (2-3 clocks: the suite answers a word in one I/O cycle; pascal100 I/O boards are
// 8-bit, so a word is two cycles here, as on a real CPU 286 with CompuPro I/O boards).
#include <cstdint>

namespace altair::i80286 {

struct TimingFacts {
    bool repFirst = true;   // REP: the first iteration of this instruction (pays the base)
    bool taken = false;     // Jcc / LOOPx / JCXZ / INTO took the branch
    int reps = 0;           // REP string op: iterations done (CX before - after)
    int shiftCount = 0;     // D2/D3: CL as it was
    int wordTransfers = 0;  // word accesses that took two bus cycles
};

inline int timing(const uint8_t* b, int len, const TimingFacts& f) {
    int i = 0;
    bool rep = false;
    for (; i < len; ++i) {   // prefixes: no clocks of their own in Intel's tables
        const uint8_t p = b[i];
        if (p == 0xF2 || p == 0xF3) rep = true;
        else if (p != 0x26 && p != 0x2E && p != 0x36 && p != 0x3E && p != 0xF0) break;
    }
    const uint8_t op = b[i];
    const uint8_t modrm = (i + 1 < len) ? b[i + 1] : 0;
    const int mod = modrm >> 6, rm = modrm & 7, reg = (modrm >> 3) & 7;
    const bool mem = mod != 3;
    const bool w = op & 1;
    // +1 for base + index + displacement: [BX+SI+d], [BX+DI+d], [BP+SI+d], [BP+DI+d].
    const int bid = (mem && mod != 0 && rm < 4) ? 1 : 0;
    auto rmT = [&](int r, int m) { return mem ? m + bid : r; };
    // The shift count: CL for D2/D3, the immediate byte (the last byte) for C0/C1.
    const int shifts = ((op == 0xC0 || op == 0xC1) ? b[len - 1] : f.shiftCount) & 0x1F;

    int t = 0;
    switch (op) {
    // ---- ALU r/m: 2, mem=7. CMP into a register: mem=6 -----------------------------
    case 0x00: case 0x01: case 0x02: case 0x03: case 0x08: case 0x09: case 0x0A: case 0x0B:
    case 0x10: case 0x11: case 0x12: case 0x13: case 0x18: case 0x19: case 0x1A: case 0x1B:
    case 0x20: case 0x21: case 0x22: case 0x23: case 0x28: case 0x29: case 0x2A: case 0x2B:
    case 0x30: case 0x31: case 0x32: case 0x33: case 0x38: case 0x39:
        t = rmT(2, 7); break;
    case 0x3A: case 0x3B: t = rmT(2, 6); break;
    case 0x04: case 0x05: case 0x0C: case 0x0D: case 0x14: case 0x15: case 0x1C: case 0x1D:
    case 0x24: case 0x25: case 0x2C: case 0x2D: case 0x34: case 0x35: case 0x3C: case 0x3D:
        t = 3; break;
    case 0x06: case 0x0E: case 0x16: case 0x1E: t = 3; break;   // PUSH sreg
    case 0x07: case 0x17: case 0x1F: t = 5; break;              // POP sreg
    case 0x27: case 0x2F: case 0x37: case 0x3F: t = 3; break;   // DAA DAS AAA AAS

    // ---- 0F: the system instructions ---------------------------------------------
    case 0x0F: {
        const uint8_t op2 = (i + 1 < len) ? b[i + 1] : 0;
        const uint8_t m2 = (i + 2 < len) ? b[i + 2] : 0;
        const bool mem2 = (m2 >> 6) != 3;
        const int reg2 = (m2 >> 3) & 7;
        if (op2 == 0x06) t = 2;                                           // CLTS
        else if (op2 == 0x01) {
            static constexpr uint8_t k01[8] = {11, 12, 11, 12, 2, 2, 3, 3}; // SGDT SIDT LGDT LIDT SMSW - LMSW
            t = k01[reg2] + (mem2 ? (reg2 == 4 ? 1 : reg2 == 6 ? 3 : 0) : 0);
        } else if (op2 == 0x00) t = (reg2 <= 1) ? (mem2 ? 3 : 2) : (mem2 ? 16 : 14);
        else t = mem2 ? 16 : 14;                                          // LAR LSL
        break;
    }

    // ---- 40-6F ----------------------------------------------------------------------
    case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
    case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
        t = 2; break;                                            // INC/DEC rw
    case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
        t = 3; break;                                            // PUSH rw
    case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5E: case 0x5F:
        t = 5; break;                                            // POP rw
    case 0x60: t = 17; break;                                    // PUSHA
    case 0x61: t = 19; break;                                    // POPA
    case 0x62: t = 13 + bid; break;                              // BOUND (in bounds)
    case 0x63: t = rmT(10, 11); break;                           // ARPL
    case 0x68: case 0x6A: t = 3; break;                          // PUSH imm
    case 0x69: case 0x6B: t = rmT(21, 24); break;                // IMUL rw,ew,imm
    case 0x6C: case 0x6D: case 0x6E: case 0x6F:                  // INS OUTS: 5, REP 5+4n
        t = rep ? (f.repFirst ? 5 : 0) + 4 * f.reps : 5; break;

    // ---- Jcc: 7, noj=3 ------------------------------------------------------------
    case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
    case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F:
        t = f.taken ? 7 : 3; break;

    // ---- 80-8F ----------------------------------------------------------------------
    case 0x80: case 0x81: case 0x82: case 0x83: t = reg == 7 ? rmT(3, 6) : rmT(3, 7); break;
    case 0x84: case 0x85: t = rmT(2, 6); break;                  // TEST
    case 0x86: case 0x87: t = rmT(3, 5); break;                  // XCHG
    case 0x88: case 0x89: case 0x8C: t = rmT(2, 3); break;       // MOV e,r / e,sreg
    case 0x8A: case 0x8B: case 0x8E: t = rmT(2, 5); break;       // MOV r,e / sreg,e
    case 0x8D: t = 3 + bid; break;                               // LEA
    case 0x8F: t = 5 + (mem ? bid : 0); break;                   // POP mw

    // ---- 90-9F ----------------------------------------------------------------------
    case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
        t = 3; break;                                            // NOP, XCHG AX,rw
    case 0x98: case 0x99: t = 2; break;                          // CBW CWD
    case 0x9A: t = 13; break;                                    // CALL far
    case 0x9B: t = 3; break;                                     // WAIT
    case 0x9C: t = 3; break;                                     // PUSHF
    case 0x9D: t = 5; break;                                     // POPF
    case 0x9E: case 0x9F: t = 2; break;                          // SAHF LAHF

    // ---- A0-AF: accumulator moves and strings ---------------------------------------
    case 0xA0: case 0xA1: t = 5; break;
    case 0xA2: case 0xA3: t = 3; break;
    case 0xA4: case 0xA5: t = rep ? (f.repFirst ? 5 : 0) + 4 * f.reps : 5; break;  // MOVS
    case 0xA6: case 0xA7: t = rep ? (f.repFirst ? 5 : 0) + 9 * f.reps : 8; break;  // CMPS
    case 0xA8: case 0xA9: t = 3; break;                                            // TEST acc
    case 0xAA: case 0xAB:                                                          // STOS
        t = rep ? (f.repFirst ? 4 : 0) + 3 * f.reps : 3;
        // REP STOSW to an odd DI: the real chip takes 1 more clock per word, not Intel's 2
        // (the suite: 4 per word odd, 3 even).
        if (rep) t -= f.wordTransfers;
        break;
    case 0xAC: case 0xAD: t = rep ? (f.repFirst ? 5 : 0) + 4 * f.reps : 5; break;  // LODS (REP: not in the book; as MOVS)
    case 0xAE: case 0xAF: t = rep ? (f.repFirst ? 5 : 0) + 8 * f.reps : 7; break;  // SCAS

    case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7:
    case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF:
        t = 2; break;                                            // MOV r,imm

    // ---- C0-CF ----------------------------------------------------------------------
    case 0xC0: case 0xC1: case 0xD2: case 0xD3: t = rmT(5, 8) + shifts; break;
    case 0xD0: case 0xD1: t = rmT(2, 7); break;
    case 0xC2: case 0xC3: t = 11; break;                         // RET near
    case 0xC4: case 0xC5: t = 7 + bid; break;                    // LES LDS
    case 0xC6: case 0xC7: t = rmT(2, 3); break;                  // MOV e,imm
    case 0xC8: {                                                 // ENTER dw,level
        const int level = ((i + 3 < len) ? b[i + 3] : 0) & 0x1F;
        t = level == 0 ? 11 : level == 1 ? 15 : 12 + 4 * level;
        break;
    }
    case 0xC9: t = 5; break;                                     // LEAVE
    case 0xCA: case 0xCB: t = 15; break;                         // RET far
    case 0xCC: case 0xCD: t = 23; break;                         // INT
    case 0xCE: t = f.taken ? 24 : 3; break;                      // INTO
    case 0xCF: t = 17; break;                                    // IRET

    // ---- D4-DF ----------------------------------------------------------------------
    case 0xD4: t = 16; break;                                    // AAM
    case 0xD5: t = 14; break;                                    // AAD
    case 0xD6: t = 3; break;                                     // SALC: undocumented; not in the book
    case 0xD7: t = 5; break;                                     // XLAT
    case 0xD8: case 0xD9: case 0xDA: case 0xDB: case 0xDC: case 0xDD: case 0xDE: case 0xDF:
        t = rmT(14, 27); break;   // ESC with no 80287: not in the book; fitted to the suite

    // ---- E0-EF ----------------------------------------------------------------------
    case 0xE0: case 0xE1: case 0xE2: case 0xE3: t = f.taken ? 8 : 4; break;  // LOOPx JCXZ
    case 0xE4: case 0xE5: case 0xEC: case 0xED: t = 5; break;    // IN
    case 0xE6: case 0xE7: case 0xEE: case 0xEF: t = 3; break;    // OUT
    case 0xE8: case 0xE9: case 0xEB: t = 7; break;               // CALL near, JMP near/short
    case 0xEA: t = 11; break;                                    // JMP far

    // ---- F0-FF ----------------------------------------------------------------------
    case 0xF4: t = 2; break;                                     // HLT
    case 0xF5: case 0xF8: case 0xF9: case 0xFB: case 0xFC: case 0xFD: t = 2; break;
    case 0xFA: t = 3; break;                                     // CLI
    case 0xF6: case 0xF7:
        switch (reg) {
        case 0: case 1: t = rmT(3, 6); break;                    // TEST e,imm
        case 2: case 3: t = rmT(2, 7); break;                    // NOT NEG
        case 4: case 5: t = w ? rmT(21, 24) : rmT(13, 16); break;// MUL IMUL
        case 6: t = w ? rmT(22, 25) : rmT(14, 17); break;        // DIV
        default: t = w ? rmT(25, 28) : rmT(17, 20); break;       // IDIV
        }
        break;
    case 0xFE: t = rmT(2, 7); break;                             // INC/DEC eb
    case 0xFF:
        switch (reg) {
        case 0: case 1: t = rmT(2, 7); break;                    // INC DEC
        case 2: case 4: t = rmT(7, 11); break;                   // CALL/JMP near indirect
        case 3: t = 16 + bid; break;                             // CALL far indirect
        case 5: t = 15 + bid; break;                             // JMP far indirect
        default: t = rmT(3, 5); break;                           // PUSH e
        }
        break;
    default: t = 3; break;                                       // not reached
    }
    return t + 2 * f.wordTransfers;   // an odd-address (or byte-serial) word: one more bus cycle
}

} // namespace altair::i80286
