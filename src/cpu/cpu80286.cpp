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

// An address no real-mode CS reaches (above 10FFEF): load CS = address/16 with the base at
// the address's 16-byte boundary, as the reset state does for FFFFF0.
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
    // it, so count queued events as a possible wake-up, as for pINT. A shutdown ignores INTR.
    return eventsQueued || (!impl_->shutdown && interruptsEnabled() && pint);
}
bool Cpu80286::shutdown() const { return impl_->shutdown; }
uint32_t Cpu80286::segBase(uint16_t seg) const {
    if (seg == impl_->registers.cs()) return impl_->csBase();
    return uint32_t(seg) << 4;
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
// CS*16 and must survive a snapshot), then what only the 80286 has: the MSW, the IDTR (a
// real-mode LIDT moves the vector table), the GDTR, the queue refill and the shutdown
// (snapshot format 9).
void Cpu80286::serialize(StateWriter& w) const {
    x86core::serialize(*impl_, w);
    w.u8((uint8_t)(high_ >> 20));
    w.u8((uint8_t)(highMask_ >> 20));
    w.u32(impl_->csBase());
    const auto& R = impl_->registers;
    w.u16(R.msw());
    for (const x86::DescriptorTablePointer& t :
         {R.get<x86::DescriptorTable::Interrupt>(), R.get<x86::DescriptorTable::Global>()}) {
        w.u16(t.limit);
        w.u32(t.base);
    }
    w.boolean(impl_->refill);
    w.boolean(impl_->shutdown);
}

void Cpu80286::deserialize(StateReader& r) {
    // CLK's set_msw() cannot clear PE: start from the reset registers, then load them all.
    impl_->registers.reset();
    x86core::deserialize(*impl_, r);
    high_ = (uint32_t)(r.u8() & 0x0F) << 20;
    highMask_ = (uint32_t)(r.u8() & 0x0F) << 20;
    const uint32_t base = r.u32() & 0xFFFFFF;
    if (base != (uint32_t(impl_->registers.cs()) << 4)) setCsBase(impl_->registers.cs(), base);
    auto& R = impl_->registers;
    R.set_msw(r.u16());
    x86::DescriptorTablePointer idt, gdt;
    idt.limit = r.u16();
    idt.base = r.u32() & 0xFFFFFF;
    gdt.limit = r.u16();
    gdt.base = r.u32() & 0xFFFFFF;
    R.set<x86::DescriptorTable::Interrupt>(idt);
    R.set<x86::DescriptorTable::Global>(gdt);
    impl_->refill = r.boolean();
    impl_->shutdown = r.boolean();
}

} // namespace altair
