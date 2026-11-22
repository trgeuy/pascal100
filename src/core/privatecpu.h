#pragma once
//
// A PROCESSOR ON A BOARD'S OWN BUS, AS THE DEBUGGER SEES IT (DESIGN.md 3.0.4, stage 4).
//
// The CompuPro SPUZ and the Jade Double D run a Z80 on a private bus and clock. That Z80 is
// not a bus master and not a unit of a CpuCard, so the run loop never steps it: the board
// does, inside its catch-up. A board that publishes such a processor to the debugger gives
// it a PrivateCpu (Board::privateCpus). The name is "<board>:<unit>" (spuz0:z80), the form
// BREAK, REGS, STEP, DISASM, HISTORY CPU and DUMP take for any core.
//
// The base class keeps what every such board needs the same way, so a second board gets it
// for nothing:
//   - PC BREAKPOINTS. Debugger::run hands each private core its breakpoints when a run starts
//     (setBreaks). The board asks shouldStop(pc) before each step. A hit HOLDS the core: the
//     board steps it no more, and its clock stops there. The run stops at the next host
//     instruction boundary (takeHit). The next run lets it go (resume), and the first step
//     from the breakpoint is not checked again, as a host breakpoint is stepped past.
//   - THE HISTORY RING, OPT-IN. A processor on a board often never halts (CompuPro's
//     scheduler loops at FD00h), so recording costs on every instruction of every board. The
//     board turns it on from a property (setHistory); off, record() is one test.
//
// What the board supplies: the core, a peek of the code bytes as the core would fetch them
// (through the board's own bus and bank), and one step that the host does not share.

#include "core/insnrec.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace altair {

class CpuCore;

class PrivateCpu {
public:
    explicit PrivateCpu(std::string unit) : unit_(std::move(unit)) {}
    virtual ~PrivateCpu() = default;

    const std::string& unit() const { return unit_; }

    virtual CpuCore& core() = 0;
    // The byte at `addr` as the core would fetch it: its own bus, its own bank. No cycle.
    virtual uint8_t peekCode(uint32_t addr) const = 0;
    // One instruction of this core, and none of the host. False, with the reason in `err`,
    // when the core cannot step (the SPUZ Z80 in reset). The board is then ahead of the
    // host; its catch-up does nothing until the host passes it. Not checked against the
    // breakpoints, and it lets a held core go.
    virtual bool step(std::string& err) = 0;

    // ---- breakpoints ----
    // The PC breakpoints bound to this core: id and the inclusive range. An empty list
    // disarms it. Only unconditional stops come here (Debugger::run); a condition or a
    // TRACE action is refused when the breakpoint is set, because both are judged against
    // the run loop's registers at a host boundary.
    struct Break {
        int      id = 0;
        uint32_t lo = 0, hi = 0;
    };
    void setBreaks(std::vector<Break> b) { breaks_ = std::move(b); }
    bool armed() const { return !breaks_.empty(); }
    // Asked by the board before each step: true, and the core is held, on a hit.
    bool shouldStop(uint32_t pc);
    bool held() const { return held_; }
    // The breakpoint id that held the core, read once (0 for none). The run loop asks it at
    // each boundary of a run with any private breakpoint armed.
    int takeHit() {
        int h = hit_;
        hit_ = 0;
        return h;
    }
    // A run starts: a held core goes on, and the breakpoint it sits on is not hit again
    // until the core has stepped once. A hit the run loop has not taken yet (the last run
    // ended at that boundary for another reason) stays held, so it is still reported --
    // unless its breakpoint is no longer in the list: call setBreaks() first.
    void resume();

    // ---- history ----
    void setHistory(bool on);
    bool historyOn() const { return historyOn_; }
    // Record the instruction about to run. The board calls it before each step.
    void record() {
        if (historyOn_) recordSlow();
    }
    // The last `n` instructions, oldest first.
    std::vector<InsnRec> history(size_t n) const;
    void clearHistory();

    static constexpr size_t kHistoryCap = 8192;  // as the host's ring (Debugger)

protected:
    // Called by step(): the core stepped by hand, so it is no longer held.
    void stepped() {
        held_ = false;
        skip_ = false;
        hit_  = 0;
    }

private:
    void recordSlow();

    std::string unit_;
    std::vector<Break> breaks_;
    bool     held_ = false;
    int      hit_  = 0;
    bool     skip_ = false;   // the next shouldStop() at skipPc_ is not a hit
    uint32_t skipPc_ = 0;

    bool historyOn_ = false;
    std::vector<InsnRec> ring_;
    size_t head_ = 0;
};

}  // namespace altair
