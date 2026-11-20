#include "cpu/cpu80286.h"

// The shared core (bus adapter, step, interrupts, REP, registers, state); see x86_impl.h.
#include "cpu/x86_impl.h"

namespace altair {

namespace x86 = InstructionSet::x86;

struct Cpu80286::Impl : x86core::Impl<x86::Model::i80286> {};

Cpu80286::Cpu80286() : impl_(std::make_unique<Impl>()) { reset(Reset::PowerOn); }
Cpu80286::~Cpu80286() = default;

uint32_t Cpu80286::pc() const { return impl_->linearPc(); }
uint32_t Cpu80286::csBase() const { return impl_->csBase(); }
bool Cpu80286::pcSegOff(uint16_t& seg, uint16_t& off) const {
    auto& R = const_cast<Impl&>(*impl_).registers;   // CLK's accessors are not const
    seg = R.cs();
    off = R.ip();
    return true;
}

// The CS descriptor cache, loaded by hand: CLK's real-mode load makes the base CS*16, and the
// 80286 at reset has a base of FF0000 under CS = F000. Built as a descriptor-table entry:
// limit FFFF, the 24-bit base, type 92 (present, ring 0, writable data: what CLK's own
// real-mode load uses).
void Cpu80286::setCsBase(uint16_t cs, uint32_t base) {
    const uint16_t words[4] = {0xFFFF, uint16_t(base & 0xFFFF),
                               uint16_t((0x92 << 8) | ((base >> 16) & 0xFF)), 0};
    impl_->registers.cs() = cs;
    impl_->segments.descriptors[x86::Source::CS] = x86::SegmentDescriptor(cs, false, words);
}

// An address no real-mode CS reaches (above FFFFF): load CS = address/16 with the base at the
// address's 16-byte boundary, as the reset state does for FFFFF0.
void Cpu80286::setPc(uint32_t v) {
    v &= 0xFFFFFF;
    if (!x86core::setPc(*impl_, v)) {
        setCsBase(uint16_t(v >> 4), v & ~0xFu);
        impl_->registers.ip() = uint16_t(v & 0xF);
        impl_->repActive = false;
    }
}
void Cpu80286::jumpFar(uint16_t cs, uint16_t ip) { x86core::jumpFar(*impl_, cs, ip); }
bool Cpu80286::repeating() const { return impl_->repActive; }
bool Cpu80286::halted() const { return impl_->halted; }
bool Cpu80286::interruptsEnabled() const { return impl_->flags.flag<x86::Flag::Interrupt>(); }
bool Cpu80286::haltCanEnd(bool pint, bool eventsQueued) const {
    // NMI* ends a halt whatever IF says; but nothing here knows whether a board will pull
    // it, so count queued events as a possible wake-up, as for pINT.
    return eventsQueued || (interruptsEnabled() && pint);
}

// Both resets (Programmer's Reference 10.4): FLAGS = 0002, MSW = FFF0, IP = FFF0, CS = F000
// with base FF0000, DS = SS = ES = 0, the IDT at 0 (CLK's registers.reset()).
void Cpu80286::reset(Reset) {
    x86core::reset(*impl_);
    impl_->registers.set_msw(0xFFF0);   // PE, MP, EM, TS = 0
    setCsBase(0xF000, 0xFF0000);
    impl_->registers.ip() = 0xFFF0;
}

// The 80286 drives all 24 lines; the card replaces the ones in highMask_.
StepResult Cpu80286::step(Bus& bus) { return x86core::step(*impl_, bus, high_, highMask_, true, local_); }

std::vector<RegDef> Cpu80286::registers() { return x86core::registers(impl_.get()); }

// The shared state, then A20-A23 from the card and the CS base (the reset base FF0000 is not
// CS*16 and must survive a snapshot).
void Cpu80286::serialize(StateWriter& w) const {
    x86core::serialize(*impl_, w);
    w.u8((uint8_t)(high_ >> 20));
    w.u8((uint8_t)(highMask_ >> 20));
    w.u32(impl_->csBase());
}

void Cpu80286::deserialize(StateReader& r) {
    x86core::deserialize(*impl_, r);
    high_ = (uint32_t)(r.u8() & 0x0F) << 20;
    highMask_ = (uint32_t)(r.u8() & 0x0F) << 20;
    const uint32_t base = r.u32() & 0xFFFFFF;
    if (base != (uint32_t(impl_->registers.cs()) << 4)) setCsBase(impl_->registers.cs(), base);
}

} // namespace altair
