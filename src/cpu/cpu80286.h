#pragma once
//
// The Intel 80286 core, REAL MODE -- the chip, not the card (DESIGN.md 3).
//
// THE INSTRUCTION SET IS NOT OURS. Execution is the x86 core of Clock Signal (CLK), model
// i80286, vendored in third_party/clk (README.pascal100.md: the commit and our fixes), under
// the same DESIGN.md 0.1 exception as the 8088. It was checked on 2026-11-19 against the
// SingleStepTests 80286 v1_real_mode suite (about 1.48 M tests recorded from a real Harris
// 80C286-12): registers and RAM match in 99.997%, the memory and I/O bus cycles in 98%. The
// 49 left are corners no working program reaches (Pascal100-Altairsim/cpu286-spike/README.md
// in the project folder).
//
// It shares everything else with the 8088/8086 (x86_impl.h: the bus adapter, step(),
// interrupts, REP one iteration per step, the register list). What the 80286 adds:
//
//   * 24 ADDRESS LINES. Real mode makes 21-bit addresses (FFFF:FFFF = 10FFEF, no wrap at
//     1 MB) and the 80286 drives A20-A23 itself. A card can replace any of A20-A23
//     (setHighAddress: the CPU 286's memory manager through jumpers J3-J6).
//   * ITS RESET STATE: CS = F000, IP = FFF0, and a hidden CS base of FF0000, so the first fetch
//     is at FFFFF0 and A20-A23 stay high until the first far jump loads CS (CPU 286 manual
//     451B p. 13). DS = SS = ES = 0, FLAGS 0002, MSW FFF0 (Programmer's Reference 10.4).
//   * A 16-BIT DATA BUS always (the 8086's one-cycle word, byte-serial when no SIXTN*).
//   * 80286 faults: invalid opcodes (INT 6), and the rest CLK raises, are delivered through
//     the vector table like any interrupt.
//
//   * ITS OWN TIME: timing80286.h, from Intel's Programmer's Reference (210498-005), checked
//     against the suite's real cycle counts.
//
// NOT MODELLED: protected mode (an LMSW that sets PE is not supported: no pascal100 guest
// needs it), LOADALL, the prefetch queue, the 80287 (ESC with no NPX).
#include "core/bus.h"
#include "cpu/cpu.h"
#include "cpu/x86local.h"

#include <cstdint>
#include <memory>

namespace altair {

class Cpu80286 : public CpuCore {
public:
    Cpu80286();
    ~Cpu80286() override;
    Cpu80286(const Cpu80286&) = delete;
    Cpu80286& operator=(const Cpu80286&) = delete;

    const char* isa() const override { return "80286"; }
    // The 80286's limit: a longer instruction (prefixes) is a general protection fault.
    uint8_t maxInsnLen() const override { return 10; }

    std::vector<RegDef> registers() override;
    void reset(Reset) override;
    StepResult step(Bus& bus) override;

    // THE PC IS THE LINEAR ADDRESS, the CS base + IP (24 bits): FFFFF0 after reset.
    uint32_t pc() const override;
    void setPc(uint32_t v) override;
    uint32_t pcMask() const override { return 0xFFFFFF; }
    bool pcSegOff(uint16_t& seg, uint16_t& off) const override;

    bool halted() const override;
    bool interruptsEnabled() const override;
    bool haltCanEnd(bool pint, bool eventsQueued) const override;

    void serialize(StateWriter& w) const override;
    void deserialize(StateReader& r) override;

    // ---- For the card ----

    // A20-A23 from the card: `value` on the lines set in `mask` (bit 0 = A20). The CPU 286's
    // memory manager drives the lines whose jumper (J3-J6) is A-C; the 80286 drives the rest.
    void setHighAddress(uint8_t value, uint8_t mask) {
        high_ = (uint32_t)(value & 0x0F) << 20;
        highMask_ = (uint32_t)(mask & 0x0F) << 20;
    }

    // The card's wait states and on-board memory (x86local.h); null = none.
    void setLocalMemory(X86LocalMemory* m) { local_ = m; }

    // CS:IP exactly, as a far jump leaves them (the CS base becomes CS*16).
    void jumpFar(uint16_t cs, uint16_t ip);

    // A REP string instruction is part way through: IP is still on it.
    bool repeating() const;

    // The CS base, as the debugger and tests see it (FF0000 after reset, CS*16 after a far jump).
    uint32_t csBase() const;

    // The 80286 comes out of reset at F000:FFF0 with a CS base of FF0000 (linear FFFFF0).
    static constexpr uint32_t kResetLinear = 0xFFFFF0;

private:
    void setCsBase(uint16_t cs, uint32_t base);

    struct Impl;
    std::unique_ptr<Impl> impl_;
    uint32_t high_ = 0;      // A20-A23 from the card, shifted
    uint32_t highMask_ = 0;  // which of them the card drives, shifted
    X86LocalMemory* local_ = nullptr;
};

} // namespace altair
