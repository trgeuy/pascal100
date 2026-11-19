#pragma once
//
// The Intel 8088 core -- the chip, not the card (DESIGN.md 3).
//
// THE INSTRUCTION SET IS NOT OURS. Execution is the x86 core of Clock Signal (CLK) by
// Thomas Harte, MIT licence, vendored in third_party/clk (README.pascal100.md there: the
// commit, and our three fixes). It was chosen by a spike on 2026-11-13: run against the
// SingleStepTests 8088 v2 suite (3,007,000 tests recorded from a real 8088), it gives the
// real chip's registers and RAM in every test, and the real chip's memory and I/O bus
// cycles, in order, in 97% (the rest: ESC with no 8087, and the undocumented SETMO).
//
// What THIS file adds is everything a pascal100 core is that CLK is not:
//
//   * THE BUS. Every byte goes through Bus& -- memReadX/memWriteX for memory (the 8088's
//     20 address lines, plus A20-A23 from the card, setHighAddress()), ioRead/ioWrite for
//     I/O (a 16-bit port). A word is two byte cycles, low byte first, as the 8088 does it;
//     a word at offset FFFF wraps inside the segment. Instruction bytes are FETCHED through
//     the bus too, the first with M1 status, so observers and boards see the fetches.
//   * TIME. CLK counts no cycles; timing8088.h does, from the bytes and what the
//     instruction did, plus a fetch floor of 4 clocks per code byte.
//   * INTERRUPTS. NMI* (edge, vector 2), pINT (level, when IF): two acknowledge cycles,
//     the vector on the second, as the 8088 runs them in minimum mode. A one-instruction
//     shadow after STI, MOV/POP to a segment register. TF gives INT 1 after the next
//     instruction.
//   * REP, ONE ITERATION PER STEP. The 8088 takes an interrupt between iterations of a
//     repeated string instruction; so does this core, because each step() does one
//     iteration and leaves IP on the prefix until CX runs out.
//
//   * THE PC IS THE LINEAR ADDRESS, CS*16+IP (20 bits, pcMask() = FFFFF): the address the
//     8088 fetches from, so a PC breakpoint stops at one place in memory, not at the same
//     IP in every segment. REGS still shows CS and IP; messages give both forms,
//     `1234:0010 (12350)` (pcSegOff, pcText). setPc() keeps CS when the address is in the
//     64K that CS covers, and otherwise loads CS = address/16, IP = address%16.
#include "core/bus.h"
#include "cpu/cpu.h"

#include <cstdint>
#include <memory>

namespace altair {

class Cpu8088 : public CpuCore {
public:
    Cpu8088();
    ~Cpu8088() override;
    Cpu8088(const Cpu8088&) = delete;
    Cpu8088& operator=(const Cpu8088&) = delete;

    // "8088", or "8086" with setWideBus(true): one instruction set, two buses.
    const char* isa() const override { return wide_ ? "8086" : "8088"; }
    // opcode, ModRM, a 16-bit displacement and a 16-bit immediate (6), plus a segment
    // override and a REP or LOCK prefix. More prefixes are legal but pointless.
    uint8_t maxInsnLen() const override { return 8; }

    std::vector<RegDef> registers() override;
    void reset(Reset) override;
    StepResult step(Bus& bus) override;

    uint32_t pc() const override;
    void setPc(uint32_t v) override;
    uint32_t pcMask() const override { return 0xFFFFF; }
    bool pcSegOff(uint16_t& seg, uint16_t& off) const override;

    bool halted() const override;
    bool interruptsEnabled() const override;
    bool haltCanEnd(bool pint, bool eventsQueued) const override;

    void serialize(StateWriter& w) const override;
    void deserialize(StateReader& r) override;

    // ---- For the card ----

    // A20-A23 of every memory cycle: the 8088 drives A0-A19, and the CPU 85/88's Memory
    // Manager latch drives the top four lines (its high nibble). 0 on a plain 1 MB system.
    void setHighAddress(uint8_t a20to23) { high_ = (uint32_t)(a20to23 & 0x0F) << 20; }

    // THE 8086: the same instructions on a 16-bit data bus. A word at an even address is one
    // 16-bit request (Bus::memRead16); with no SIXTN* answer the byte-serial pair follows,
    // even byte first, as the CPU 86/87 runs it. A word at an odd address is two byte
    // cycles, as on the chip. Only a word that took two cycles costs the 8088's extra 4
    // clocks (timing8088.h's base is the 8086's). The prefetch queue (6 bytes on the 8086, 4
    // on the 8088) is not modelled in either.
    void setWideBus(bool on) { wide_ = on; }

    // CS:IP exactly, as a far jump leaves them. The CPU 86/87's power-on jump jams
    // JMPF ss00:0000 (setPc() would keep CS when the target is inside its 64K).
    void jumpFar(uint16_t cs, uint16_t ip);
    bool wideBus() const { return wide_; }

    // A REP string instruction is part way through: IP is still on it, and the next step()
    // does its next iteration.
    bool repeating() const;

    // The 8088 comes out of reset at FFFF:0000 (linear FFFF0).
    static constexpr uint32_t kResetLinear = 0xFFFF0;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    uint32_t high_ = 0;  // A20-A23, already shifted
    bool wide_ = false;  // the 8086's 16-bit data bus
};

} // namespace altair
