#pragma once
// THE CARD'S HAND ON ITS x86 CORE'S MEMORY CYCLES (the CPU 286; x86_impl.h calls it).
//
//   * WAIT STATES. cycle() runs before every memory cycle the core makes, so the card can add
//     its wait states (Bus::addWaitStates) by the kind of cycle (the CPU 286's J1).
//   * ON-BOARD MEMORY. A card with memory behind its own S-100 buffers (the CPU 286's
//     EPROM/SRAM sockets in page FF, J7) answers cycle() true for those addresses; the core
//     then reads and writes them here and runs no S-100 cycle. A 16-bit word in it is one
//     cycle (the sockets are 16 bits wide).
//
// Addresses are physical: A20-A23 already as the card drives them.
#include <cstdint>

namespace altair {

struct X86LocalMemory {
    virtual ~X86LocalMemory() = default;
    // Before each memory cycle: add the card's wait states; true = the card's own memory.
    virtual bool cycle(uint32_t phys, bool write) = 0;
    // The same question with no side effects (the decode look-ahead, the debugger).
    virtual bool owns(uint32_t phys) const = 0;
    virtual uint8_t read(uint32_t phys) = 0;
    virtual void write(uint32_t phys, uint8_t v) = 0;
    virtual uint8_t peek(uint32_t phys) const = 0;
};

} // namespace altair
