#pragma once
//
// CpuCore -- registers and execute. THE CHIP, NOT THE CARD (DESIGN.md 3).
//
// A core is a plain object. It is NOT a Board: it has no slot, decodes no
// address, owns no clock crystal, and cannot be pulled out of the backplane with
// your hand. All of that belongs to the card that carries it (src/boards/), and
// the separation is what lets two different 8080 cards -- one with an onboard
// serial port, one without -- share this file completely.
//
// It is also what makes the validation gate (DESIGN.md 3.2) easy: TST8080 and
// 8080EXM test the CHIP, and here the chip is a thing you can hold on its own.

#include "core/bus.h"

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace altair {

class StateWriter;  // core/statefile.h -- SNAPSHOT/RESTORE
class StateReader;

// ---------------------------------------------------------------------------
// REGISTERS ARE REFLECTION -- the same trick as Board::properties() (DESIGN.md
// 3.0.3, 5).
//
// A core describes its own registers; it does NOT expose a struct the monitor
// knows the shape of. So REGS, SET REG A=3F, breakpoint conditions, SNAPSHOT and
// the MCP schema are written ONCE, generically, and a Z80 or a 6502 gets every
// one of them on the day it lands with no change to the monitor.
//
// This is the same bet that already paid for SET/SHOW/TOML/MCP. There is one
// schema and no second copy to drift.
// ---------------------------------------------------------------------------
// HOW a register wants to APPEAR. The monitor prints a one-line, DDT/SID-style
// status at every stop:
//
//     C0Z1M0E1I0 A=3F BC=0000 DE=00FF HL=8000 SP=0100 IE=1 PC=0102  MOV A,B
//
// The LAYOUT of that line -- which registers pair up, which ones are lamps, what
// each is called, and in what order -- is the CORE's to describe, because it is
// genuinely different for a Z80 or a 6502. But it is the MONITOR's to render, and
// the monitor still knows nothing: it reads `show`, `label` and `bits`, and that
// is all. Reflection holds (3.0.3); we did not hand the core a printf.
enum class RegShow {
    Flag,   // a lamp: LABEL then one digit, no spaces, clustered at the FRONT
    Field,  // LABEL=hex, printed in the order the core listed it
    Off,    // reachable BY NAME (SET REG, breakpoints, MCP) but not on the line
};

struct RegDef {
    // NAME is identity -- what you TYPE, and what SET REG and a breakpoint
    // condition look up. LABEL is presentation -- what the status line CALLS it.
    // Keeping them apart is what lets the 8080 print DDT's `M` for the sign flag
    // without `SET REG M=1` quietly becoming a thing.
    std::string name;   // "A", "PC", "SP", "F", "HL", "CY"
    int bits = 8;       // 1, 8 or 16 -- decides the width SHOW prints and what fits
    std::string label;  // what the status line calls it; empty -> name
    RegShow show = RegShow::Field;
    std::string help;   // "accumulator", "program counter"
    std::function<uint32_t()> get;
    std::function<void(uint32_t)> set;

    // Which STATUS LINE this register prints on. Almost every core is content with
    // one line (0). A core with more registers than fit a terminal width -- the Z80
    // -- puts its overflow on line 1, and the monitor wraps there, keeping the same
    // DDT/SID field format on each line (flags-first, the PC line last). Reachable-
    // by-name registers (RegShow::Off) ignore this; they never print.
    int line = 0;

    // The character between LABEL and value on the status line -- normally `=`
    // (`SP=0100`), but `'` for the Z80's shadow bank so it reads `BC'0000`. A lamp
    // (RegShow::Flag) ignores it; it prints its label then a single digit.
    char sep = '=';

    // A LAMP SPELLED OUT. DDT packs the 8080's flags as `C0Z1M0E1I0`, and that is what an 8080
    // user knows. Nine two-letter 8088 flags packed the same way read as a string of digits
    // (`OF0DF0IF0...`), so a core can ask for `OF=0 DF=0 ...` instead: label, sep, the digit,
    // and a space between lamps.
    bool spelled = false;

    const std::string& shown() const { return label.empty() ? name : label; }
};

class CpuCore {
public:
    virtual ~CpuCore() = default;

    // The INSTRUCTION SET this core speaks -- a key into disassemblerFor().
    // DISASM asks the machine, the machine asks the active core, and so you never
    // type CPU= (DESIGN.md 3.0.2). On the dual-CPU card, when the guest OUTs to
    // switch from the 8080 to the 8085, DISASM follows automatically -- because
    // "which instruction set" and "which core is active" are the same question,
    // and it is already being asked.
    virtual const char* isa() const = 0;
    // The 8086 and the 8088 run one instruction set; only the bus differs. Every place that
    // treats a core as x86 (CS:IP, the 8088 disassembler) asks this, not for one name.
    static bool isX86(const std::string& isa) { return isa == "8088" || isa == "8086" || isa == "80286"; }

    // The LONGEST instruction in that set, in bytes: opcode, prefixes and operands. CPU
    // HISTORY peeks this many code bytes before EVERY instruction, so it is the exact
    // maximum, not a round number -- each extra byte is a peek per instruction (measured
    // 2026-11-09: 8 bytes instead of 4 cost the Pascal-100 22% of its run speed).
    // It must not be over Debugger::kInsnBytes (a test checks every core).
    virtual uint8_t maxInsnLen() const = 0;

    virtual std::vector<RegDef> registers() = 0;

    // registers()' VALUES, in registers()' order -- exactly what get() on every RegDef
    // would return, without building the list. CPU HISTORY snapshots the machine before
    // EVERY instruction, and walking twenty std::function getters to do it cost more
    // than executing the instruction did.
    //
    // The default IS that walk, so a new core is correct the day it lands; a core on
    // the hot path overrides it with plain loads. An override is a second copy of the
    // ORDER, so a test checks it against registers(): tests/test_cpu.cpp for the 8080,
    // 8085 and Z80, tests/test_pmachine.cpp for the P-machine.
    virtual void captureRegs(std::vector<uint32_t>& out) {
        std::vector<RegDef> regs = registers();
        out.resize(regs.size());
        for (size_t i = 0; i < regs.size(); ++i) out[i] = regs[i].get();
    }

    // Both resets: PC<-0 and interrupts off. NEITHER TOUCHES MEMORY (DESIGN.md 6)
    // -- a core has no memory to touch, which is the tidiest possible proof.
    virtual void reset(Reset) = 0;

    // Run ONE instruction, originating real bus cycles. Everything the core knows
    // about the outside world arrives through that Bus& and nothing else: no
    // pointer to RAM, no back door. So a fetch from an empty socket reads 0xFF and
    // executes as RST 7, exactly as the metal does, with no special case.
    virtual StepResult step(Bus&) = 0;

    // PC gets named accessors on top of the reflection layer because the debugger
    // asks for it on EVERY step and every core has one. Everything else goes
    // through registers().
    //
    // THE PC IS THE ADDRESS THE CORE FETCHES ITS NEXT INSTRUCTION FROM, as the debugger
    // sees it: what a PC breakpoint matches, what the trace records, what STEP prints.
    // 32 bits wide so that one interface serves every core: 16 bits on the 8080/8085/Z80
    // and the MicroEngine (its IPC), 20 bits on the 8088 (the linear address CS*16+IP).
    // pcMask() says how many: a core with more than 16 address bits overrides it. A PC
    // given to setPc() is already inside pcMask(); the caller checks the range.
    virtual uint32_t pc() const = 0;
    virtual void setPc(uint32_t) = 0;
    virtual uint32_t pcMask() const { return 0xFFFF; }

    // A SEGMENTED PC (the 8088): the segment and offset that pc() was made from, so a
    // message can print `1234:0010 (12350)` as DEBUG and DDT-86 print CS:IP. False on a
    // core whose PC is one flat number; pcText() then gives four hex digits.
    virtual bool pcSegOff(uint16_t& seg, uint16_t& off) const {
        (void)seg; (void)off;
        return false;
    }
    // Where segment `seg` starts, as the core fetches from it (a segmented core only): the
    // linear address seg:0000. seg*16 on the 8088 and 8086; the 80286 keeps the CS base it
    // has, which is not CS*16 after reset (FF0000). Add the offset, then mask with pcMask().
    virtual uint32_t segBase(uint16_t seg) const { return uint32_t(seg) << 4; }

    virtual bool halted() const = 0;
    virtual bool interruptsEnabled() const = 0;

    // CAN AN INTERRUPT STILL END THIS HALT? Asked only while halted(). A halted processor
    // fetches nothing, so the front panel's RUN, EXAMINE and DEPOSIT do nothing; only an
    // interrupt the processor will take, or RESET, ends it. `pint` is the bus's interrupt
    // line now; `eventsQueued` says a board still has something scheduled, which may raise
    // it later. Each core answers for its own interrupt system -- the 8085's TRAP is taken
    // whatever INTE says, so "interrupts are off" alone is not the answer.
    virtual bool haltCanEnd(bool pint, bool eventsQueued) const = 0;

    // SNAPSHOT/RESTORE (DESIGN.md 13). Every architectural register AND the hidden
    // micro-state that registers() does not expose -- the EI-after-next latch, the
    // mid-INTA fetch flag, and on a Z80 the WZ/MEMPTR latch, IFF2, the interrupt
    // mode and the alternate bank. registers() is enough for REGS and SET REG; it
    // is NOT enough to resume execution cycle-for-cycle, so a core writes its state
    // explicitly here rather than being walked generically.
    virtual void serialize(StateWriter& w) const = 0;
    virtual void deserialize(StateReader& r) = 0;
};

// ---------------------------------------------------------------------------
// A CARD THAT CARRIES PROCESSORS. This is how the debugger finds the CPU without
// knowing what board it is on, or what chip it is.
//
// It is deliberately not "the CPU board" -- a card may carry more than one core
// (DESIGN.md 3.0.1), and it may carry a great deal that is not a core at all: a
// serial port, a boot PROM. Those are the card's business. This interface asks
// exactly one question, which is the only one the debugger has any right to ask.
// ---------------------------------------------------------------------------
class CpuCard {
public:
    virtual ~CpuCard() = default;

    // The core that is RUNNING. On a dual-processor card the guest can change the
    // answer with an OUT, and everything downstream -- REGS, DISASM, breakpoints --
    // follows automatically, because they all ask this same question every time
    // rather than caching what they found once.
    virtual CpuCore* activeCore() = 0;

    // EVERY core on the board, with its unit name (the same name units() gives it), in a
    // fixed order. A plain board has one, and the default says so with an EMPTY name:
    // CpuCard is not a Board and cannot ask units(), so Machine::coreByRef takes the name
    // of a one-core board from its units() instead. A board with more than one core
    // overrides this AND coreCount() (pascal100 Stage 3; the Pascal-100's `z80` and `me`).
    struct CoreUnit {
        std::string unit;
        CpuCore* core = nullptr;
    };
    virtual std::vector<CoreUnit> cores() { return {{"", activeCore()}}; }

    // How many cores -- cores().size(), without building the list. The run loop asks it on
    // every run() (one per STEP), so it must not allocate. Override it with cores().
    virtual size_t coreCount() const { return 1; }

    // APPLY A PENDING SWITCH, AT AN INSTRUCTION BOUNDARY. The run loop calls this between
    // instructions -- after the clock has advanced and DMA has run, before it records the
    // next instruction -- and only on a board with more than one core. Machine::cpu() also
    // calls it, so whoever asks for the active core between runs gets the one that runs
    // next. It must be cheap and idempotent; the default does nothing. A switch the guest makes with an OUT already shows in
    // activeCore() when the instruction retires; this is for the switch nothing executed:
    // on the Pascal-100 an NMI, or pINT with auto-transfer on, resets the processor-select
    // flip-flop U208B and hands control to the Z80 (manual ch. IV M, O). Those wires move
    // while the clock advances, after the step, so no step could report them.
    virtual void settleSwitch() {}

    // THE BYTE OF CODE AT `addr` AS CORE `core` WOULD FETCH IT, without a bus cycle. The
    // default is the physical byte at `addr`, which is right for every one-core board.
    // A card with a map or a processor that is not byte-addressed overrides it (the
    // Pascal-100: the MicroEngine fetches byte SEGB*2 + IPC through its Map, the Z80
    // through its own Map). The run loop asks it only on a card with more than one core,
    // or with a core whose PC is wider than 16 bits; the monitor asks it for the
    // instruction at the PC. A PC past 16 bits (the 8088's linear address) goes out on
    // A16-A19 too, so the default uses peekX. A card that drives A20-A23 itself (the
    // CPU 85/88's Memory Manager) overrides this to add them.
    virtual uint8_t peekCodeByte(CpuCore* core, const Bus& bus, uint32_t addr) {
        (void)core;
        return addr > 0xFFFF ? bus.peekX(addr) : bus.peek((uint16_t)addr);
    }

    // ---- THE ACHIEVED CRYSTAL. A DIAGNOSTIC, AND IT IS HOST-MEASURED. ----
    //
    // clock_hz is the crystal you ASKED for; this is the one the machine actually
    // turned -- T-states retired per REAL second, last time the run loop ran. It is
    // what made bug #6 legible in one line: "asked 8 MHz, reached 3.9". SHOW reads it
    // back as a read-only companion to clock_hz on the same card.
    //
    // The run loop times ITSELF against the host wall clock and reports the number
    // here; nothing in the simulator ever asks the host what time it is (core/clock.h),
    // and the card only stores what it was handed. So this stays off the Clock, whose
    // whole invariant is that emulated time is a pure function of the instruction
    // stream -- a host-measured rate living there would be a determinism trap waiting
    // for the first board that read it.
    //
    // Defaulted, not pure: a CpuCard that carries no crystal (or that nobody paces)
    // simply never reports, and SHOW reads 0 -- "it has not run", which is the truth.
    virtual void      reportAchievedHz(long long) {}
    virtual long long achievedHz() const { return 0; }
};

// The PC as a message prints it, in hex: `0100` on a flat 16-bit core, `1234:0010 (12350)`
// on a segmented one (CpuCore::pcSegOff), so the CS:IP the registers show and the linear
// address a breakpoint takes are both there.
inline std::string pcText(const CpuCore& c) {
    char b[24];
    uint16_t seg = 0, off = 0;
    if (c.pcSegOff(seg, off))
        std::snprintf(b, sizeof b, "%04X:%04X (%05X)", (unsigned)seg, (unsigned)off,
                      (unsigned)(c.pc() & c.pcMask()));
    else
        std::snprintf(b, sizeof b, "%04X", (unsigned)c.pc());
    return b;
}

} // namespace altair
