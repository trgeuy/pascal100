#include "core/privatecpu.h"

#include "cpu/cpu.h"

#include <algorithm>

namespace altair {

bool PrivateCpu::shouldStop(uint32_t pc) {
    if (held_) return true;
    if (skip_) {
        skip_ = false;
        if (pc == skipPc_) return false;
    }
    for (const Break& b : breaks_)
        if (pc >= b.lo && pc <= b.hi) {
            held_ = true;
            hit_  = b.id;
            return true;
        }
    return false;
}

void PrivateCpu::resume() {
    if (!held_) return;
    // A hit not taken yet is still reported -- while its breakpoint is still set. One that
    // BREAK CLEAR (or a disable) removed is dropped, and the core goes on.
    if (hit_ != 0 && std::any_of(breaks_.begin(), breaks_.end(),
                                 [this](const Break& b) { return b.id == hit_; }))
        return;
    held_   = false;
    hit_    = 0;
    skip_   = true;
    skipPc_ = core().pc();
}

void PrivateCpu::setHistory(bool on) {
    historyOn_ = on;
    if (!on) clearHistory();
}

void PrivateCpu::clearHistory() {
    ring_.clear();
    ring_.shrink_to_fit();
    head_ = 0;
}

// The same snapshot the run loop takes for a host core (Debugger::run): the registers as
// plain loads, and the core's longest instruction from where it fetches.
void PrivateCpu::recordSlow() {
    InsnRec* slot;
    if (ring_.size() < kHistoryCap) {
        ring_.emplace_back();
        slot = &ring_.back();
    } else {
        slot  = &ring_[head_];
        head_ = (head_ + 1) % kHistoryCap;
    }
    CpuCore& c  = core();
    slot->core  = &c;
    slot->pc    = c.pc();
    c.captureRegs(slot->regs);
    slot->nbytes = (uint8_t)std::min<size_t>(c.maxInsnLen(), slot->bytes.size());
    for (uint8_t i = 0; i < slot->nbytes; ++i)
        slot->bytes[i] = peekCode((slot->pc + i) & c.pcMask());
}

std::vector<InsnRec> PrivateCpu::history(size_t n) const {
    std::vector<InsnRec> out;
    const size_t have  = ring_.size();
    const bool   full  = have == kHistoryCap;
    const size_t start = full ? head_ : 0;
    const size_t skip  = have > n ? have - n : 0;
    for (size_t i = skip; i < have; ++i) out.push_back(ring_[(start + i) % have]);
    return out;
}

}  // namespace altair
