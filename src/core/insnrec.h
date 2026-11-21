#pragma once
//
// ONE RECORDED INSTRUCTION -- the CPU history's unit (HISTORY CPU). Two recorders fill it:
// the run loop for the host's cores (Debugger::insnHistory), and a board for a processor on
// its own bus (PrivateCpu::history, DESIGN.md 3.0.4). It knows no instruction set; the
// monitor formats it (Monitor::renderInsn).

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace altair {

class CpuCore;

// The most code bytes one history record holds (InsnRec::bytes).
inline constexpr size_t kInsnBytes = 16;

struct InsnRec {
    // THE CORE THAT RAN IT. On a board with two processors (DESIGN.md 3.0.1; the
    // Pascal-100) the active core changes during a run, so the record must say whose
    // registers `regs` holds and whose instruction set decodes `bytes` -- the core that
    // is active when you LOOK may not be the one that ran it. Never dangling: the ring
    // is cleared whenever a CPU board leaves the machine (Machine::remove/clear/replaceWith).
    CpuCore* core = nullptr;
    uint32_t pc = 0;
    std::vector<uint32_t> regs;      // one value per RegDef, in registers() order
    // The opcode and its operands at pc: the core's maxInsnLen() bytes (3 for the
    // 8080/8085, 4 for the Z80 and P-code), so a 4-byte Z80 instruction shows its last
    // byte (altairsim records 3 for every core). The array is sized for the cores on
    // the roadmap: the 8088 (6 bytes plus prefixes) and maybe the 68000 (10 bytes).
    // Its size costs ring memory only; the time cost is nbytes peeks per instruction.
    std::array<uint8_t, kInsnBytes> bytes{};
    uint8_t nbytes = 0;
};

}  // namespace altair
