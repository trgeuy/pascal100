#include "cpu/cpu8088.h"

// The core itself (bus adapter, step, interrupts, REP, registers, state) is shared with the
// 80286 and lives in x86_impl.h, the only place CLK is included.
#include "cpu/x86_impl.h"

namespace altair {

namespace x86 = InstructionSet::x86;

struct Cpu8088::Impl : x86core::Impl<x86::Model::i8086> {};

Cpu8088::Cpu8088() : impl_(std::make_unique<Impl>()) { reset(Reset::PowerOn); }
Cpu8088::~Cpu8088() = default;

uint32_t Cpu8088::pc() const { return impl_->linearPc(); }
bool Cpu8088::pcSegOff(uint16_t& seg, uint16_t& off) const {
    auto& R = const_cast<Impl&>(*impl_).registers;   // CLK's accessors are not const
    seg = R.cs();
    off = R.ip();
    return true;
}
void Cpu8088::setPc(uint32_t v) { (void)x86core::setPc(*impl_, v); }
void Cpu8088::jumpFar(uint16_t cs, uint16_t ip) { x86core::jumpFar(*impl_, cs, ip); }
bool Cpu8088::repeating() const { return impl_->repActive; }
bool Cpu8088::halted() const { return impl_->halted; }
bool Cpu8088::interruptsEnabled() const { return impl_->flags.flag<x86::Flag::Interrupt>(); }
bool Cpu8088::haltCanEnd(bool pint, bool eventsQueued) const {
    // NMI* ends a halt whatever IF says; but nothing here knows whether a board will pull
    // it, so count queued events as a possible wake-up, as for pINT.
    return eventsQueued || (interruptsEnabled() && pint);
}

void Cpu8088::reset(Reset) { x86core::reset(*impl_); }

// The 8088 drives A0-A19; the card's latch drives all of A20-A23.
StepResult Cpu8088::step(Bus& bus) { return x86core::step(*impl_, bus, high_, 0xF00000u, wide_); }

std::vector<RegDef> Cpu8088::registers() { return x86core::registers(impl_.get()); }

void Cpu8088::serialize(StateWriter& w) const {
    x86core::serialize(*impl_, w);
    w.u8((uint8_t)(high_ >> 20));
}

void Cpu8088::deserialize(StateReader& r) {
    x86core::deserialize(*impl_, r);
    high_ = (uint32_t)(r.u8() & 0x0F) << 20;
}

} // namespace altair
