#pragma once
//
// The S-100 bus (DESIGN.md 4) -- the load-bearing spec.
//
// THE RULE THIS FILE EXISTS TO ENFORCE:
//   The bus carries signals and moves bytes. It does not invent behavior.
//
// It arbitrates no overlay, vectors no interrupt, knows no bank, and has never
// heard of ROM. Every one of those lives in a board. When you are tempted to
// add "if (board is a ROM)" here, you have found a bug in your board instead.

#include "core/busclock.h"

#include <bitset>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace altair {

class Board;
class Bus;

enum class Cycle { MemRead, MemWrite, IoRead, IoWrite, IntAck };

// THE BUS (reference/IEEE_696_1983.md): ANSI/IEEE Std 696-1983, and nothing else. 24
// address lines (A16-A23 = BusCycle::ext), 16-bit I/O addresses (A0-A15), and the 16-bit
// transfer (sXTRQ*/SIXTN*). altairsim also has the Altair bus (16 address lines, 8-bit
// ports); pascal100 took it out in standalone step 3b (2026-11-09), so there is no bus
// type to choose and no board is refused for its bus.

// THE 8080 STATUS WORD -- a BUS SIGNAL, not a lamp (Intel 8080 / Altair 8800
// Operator's Manual §4, p.32). The CPU latches these eight lines at SYNC, the S-100
// backplane carries them, and a front panel merely DISPLAYS them. They cross to the
// panel bridge verbatim (see boards/frontpanel-link.h) -- the bit order below is the
// wire contract in altairsim-fp/docs/panel-protocol.md.
//
// WO* IS ACTIVE LOW: the bit is 1 during a read/input and 0 during a write/output.
// That is the SIGNAL, and it crosses verbatim. The real Altair panel displays WO*
// RAW, with no inversion -- the WO lamp is LIT during a read (and most cycles, since
// fetches are reads) and DARK during a write. An M1 opcode fetch, being a memory
// read, lights MEMR, M1 and WO together. So neither side inverts: we emit the
// active-low signal and the panel lights the lamp straight from it.
//
// M1 and STACK are asserted BY THE CPU: only the processor knows an opcode fetch from
// an operand read, or a stack machine cycle from any other memory access. It passes the
// whole word to memRead/memWrite (see Bus's transfer functions and cpu8080.cpp), and
// settle() ORs the type-derived fallback on -- a subset, so it never clobbers them. HLTA
// stays 0 here: it is a machine-control indicator the bridge composes from the halt flag,
// not a bus status bit (see mits-frontpanel.h). NOTE the Z80 has no STACK output, so a
// Z80 never sets StStack (its push/pop are ordinary memory cycles) -- see cpuZ80.cpp.
enum Status8080 : uint8_t {
    StInta  = 0x01,  // D0 -- interrupt acknowledge
    StWo    = 0x02,  // D1 -- WO*, ACTIVE LOW: 1=read/input, 0=write/output
    StStack = 0x04,  // D2 -- stack access        (CPU-asserted; 8080 only, never Z80)
    StHlta  = 0x08,  // D3 -- halt acknowledge    (deferred: bridge composes from flags)
    StOut   = 0x10,  // D4 -- output (I/O write)
    StM1    = 0x20,  // D5 -- opcode fetch        (CPU-asserted)
    StInp   = 0x40,  // D6 -- input (I/O read)
    StMemR  = 0x80,  // D7 -- memory read
};

struct BusCycle {
    Cycle type = Cycle::MemRead;
    uint16_t addr = 0;  // memory address; for I/O the port is addr & 0xFF
    // Valid on writes. On a READ it is zero while the cycle is in flight -- nobody
    // has driven the bus yet when decodes() and read() are asked -- but it is
    // BACK-FILLED with the byte that came back (a board's, or the floating bus's
    // 0xFF) before snoop() and the observers see it. See Bus::settle().
    uint8_t data = 0;

    // Set by the bus's first pass, from whatever board pulled PHANTOM* (pin 67).
    // A board strapped to honor it takes ITSELF off the bus for this cycle --
    // the bus picks no winner. That is the entire overlay mechanism.
    bool phantom = false;

    // The 8080 status word (Status8080 above) -- the S-100 status lines for this
    // cycle. LATCHED by Bus::settle() from the cycle type, exactly where `data` is
    // back-filled, so a snooper/observer sees it valid. A bus master (the CPU) may
    // set enrichment bits (M1, ...) at the origin; settle() ORs the type-derived
    // base onto them, so it never clobbers what the origin already knew.
    uint8_t status = 0;

    // ---- IEEE 696 extensions ----
    //
    // A16-A23 of a memory cycle. A board that reads only `addr` ignores these lines and
    // so answers in every 64K segment: IEEE 696's behaviour for a slave that does not
    // decode the extended address (2.2.2.4), with no change to the board.
    uint8_t ext = 0;
    // sXTRQ* (pin 58): the master asks for a 16-bit transfer. Set only on the one cycle
    // of a 16-bit transfer that a slave answered with SIXTN* (Bus::memRead16).
    bool xtrq = false;
    // The ODD-addressed byte of that 16-bit cycle (OD7-OD0 on DI7-DI0, 2.2.4, 2.6). `data`
    // then holds the EVEN-addressed byte (ED7-ED0 on DO7-DO0). Like `data`, back-filled
    // on a read before snoop() and the observers see the cycle.
    uint8_t dataHi = 0;

    // `addr` of an I/O cycle carries all of A0-A15; a board decodes the port from A0-A7
    // (board.h).
    uint8_t port() const { return (uint8_t)(addr & 0xFF); }
    // The physical 24-bit address of a memory cycle.
    uint32_t phys() const { return ((uint32_t)ext << 16) | addr; }
    bool isWrite() const { return type == Cycle::MemWrite || type == Cycle::IoWrite; }
};

// THE ADDRESS TRANSLATOR. A processor board with a memory map between its core and the bus
// (the Pascal-100's Map, Stage 4) turns the core's 16-bit LOGICAL address into a physical
// one. Bus::memRead/memWrite (the 16-bit forms, which the CPU cores use) run through it
// while one is set; the bus then cuts the result to its own width (16 bits on the Altair
// bus, 24 on IEEE 696). The X forms, peek, the monitor and DMA are physical and never
// pass through it. The board sets it only around its own core's step, with a
// TranslatorScope (below), so nothing else is ever translated.
struct AddressTranslator {
    virtual ~AddressTranslator() = default;
    virtual uint32_t translate(uint16_t logical, Cycle type) = 0;

    // The same board's hand on its core's I/O cycles: the core's own A0-A15 in, what the
    // board puts on the bus out. The Pascal-100 copies the port onto A8-A15 (its address
    // mirror, manual IV K) and keeps the Z80's B for its Map port. Default: unchanged.
    virtual uint16_t ioAddress(uint16_t addr, Cycle) { return addr; }

    // The byte a translated memory READ hands the core, after the cycle has run. The
    // Pascal-100's jump-on-reset (manual IV P) replaces the data of the Z80's first three
    // reads while the cycles still go out to the bus. Default: the byte the bus drove.
    virtual uint8_t readData(uint8_t v) { return v; }

    // THE CARD'S OWN I/O. A device on the master's local bus, behind the card's S-100
    // buffers: the CPU 86/87's 80130 and Memory Manager, which decode A8-A15 (FFF0-FFFF,
    // 00FD). The cycle still runs on the S-100 bus -- address and status go out, every
    // board and observer sees it, and an 8-bit board that decodes A0-A7 answers it as it
    // would on a real backplane -- but it is not UNCLAIMED, and the core takes the local
    // byte on a read (the card's buffers are off, ACK*; observers see the floating bus).
    // ownsIo() is a pure question about the address (`addr` after ioAddress()); then the
    // bus calls localRead() or localWrite() once, after the bus cycle. This keeps the
    // board contract (an I/O decode on the BUS uses A0-A7 only, board.h).
    virtual bool ownsIo(uint16_t, Cycle) const { return false; }
    virtual uint8_t localRead(uint16_t) { return 0xFF; }
    virtual void localWrite(uint16_t, uint8_t) {}
};

// The result of a 16-bit transfer request (Bus::memRead16 and the byte-serial helper).
// Bytes by ADDRESS, not by significance: the even-addressed and the odd-addressed byte.
// Which of the two is the high-order byte of a word is the master's business.
struct Transfer16 {
    bool sixtn = false;  // a slave answered SIXTN*: ONE 16-bit cycle ran
    uint8_t even = 0xFF;
    uint8_t odd  = 0xFF;
};

// Thrown out of a cycle function BEFORE the device is touched, to unwind the
// in-flight instruction when a cycle breakpoint should stop the machine WITH THE
// PC STILL ON THE INSTRUCTION (see Bus::setPreAccessVeto and the debugger's run
// loop). It carries nothing: the veto predicate has already recorded which
// breakpoint fired. This is a deliberate, localized departure from the codebase's
// no-sentinels idiom -- it is the only way to abandon a bus access from deep inside
// a core's decode switch without every core learning what a breakpoint is -- and it
// is confined to the bus<->debugger seam and caught one frame up in Debugger::run.
struct CycleBreakBefore {};

// NOTE there is deliberately NO `origin` field (Cpu/Monitor/Dma). A real
// backplane cycle carries no such tag -- which is exactly WHY a front-panel
// DEPOSIT is indistinguishable from a CPU write, and why a real ROM ignores
// both. See DESIGN.md 10.2: the operator writes ROM through Machine::burn(),
// which is a PROM burner and not a bus operation at all.

enum class Reset {
    PowerOn,  // POC* -- Altair bus pin 76. Board-specific; the board just needs
              //   to know it happened. Guest code cannot see it.
    Bus       // RESET* -- the front-panel button.
};
// NEITHER RESET CLEARS MEMORY. Only removing power does (DESIGN.md 6). A RAM
// chip has no POC* pin.

enum class Contention { Silent, Warn, Error };

// The floating-bus diagnostic (DESIGN.md 4.6.1). A guest that hits a board that
// isn't there reads 0xFF for ever and hangs with no explanation -- this names the
// port and the PC when it happens. I/O only: guests scan MEMORY constantly and it
// is normal, so memory cycles are never warned. Default Silent -- opt-in, so no
// existing machine gains a line and the hot I/O path pays one enum compare when
// off. Warn logs and runs on; Halt logs AND stops the guest at the boundary.
enum class Unclaimed { Silent, Warn, Halt };

// ---------------------------------------------------------------------------
// BOARDS RESPOND TO BUS CYCLES. A BUS MASTER ORIGINATES THEM. (DESIGN.md 3)
//
// Two concepts, and a CPU card is both. This lives with the bus and not with the
// CPU on purpose: S-100 has pHOLD/pHLDA precisely BECAUSE a backplane can have
// more than one master, so when a DMA card arrives (a Dazzler, a disk controller
// stealing cycles) it is a Board that BECOMES a BusMaster when granted the bus,
// driving the very same cycles through the very same interface. DMA is then not
// a bolted-on path through the bus -- it is the mechanism the CPU already uses.
// ---------------------------------------------------------------------------
enum class RunStatus {
    Ok,      // an instruction retired
    Halted,  // HLT. The CPU is still powered and still watching pINT.
};

// An explicit result, NEVER a sentinel T-state count (DESIGN.md 3.1, 16). The
// prototype's `return 0 means something went wrong` is exactly how the RLC/RRC
// bug hid: a legal instruction that happened to take zero cycles was
// indistinguishable from a failure, so nobody looked.
struct StepResult {
    uint32_t tStates = 0;
    RunStatus status = RunStatus::Ok;
};

class BusMaster {
public:
    virtual ~BusMaster() = default;
    virtual StepResult step(Bus&) = 0;
};

// A row of SHOW BUS MAP / SHOW BUS IO.
struct MapEntry {
    uint32_t lo = 0, hi = 0;  // inclusive
    std::string what;         // "ram", "rom", "read/write"
    std::string note;         // "bank 3", "phantom:all", "empty socket"
};

class Bus {
public:
    void attach(Board* b);
    void detach(Board* b);
    const std::vector<Board*>& boards() const { return boards_; }

    // ---- The decode is CACHED, because on real hardware it is WIRED ----
    //
    // A card's address decoder is combinational logic -- a PAL, a row of gates --
    // wired to the address lines and to the status lines sMEMR/sINP/sOUT. It does
    // not "answer a question" per cycle. It settles, and it only CHANGES when
    // something latches: a bank strap, PHANTOM*, a card pulled from the backplane.
    //
    // Re-deriving it on every cycle was costing more than everything else in the
    // simulator combined, AND it was asking cards questions they are not even
    // wired for: an I/O-only 2SIO was asked to decode every memory read, and its
    // first act was to throw the question away (mits-2sio.cpp:296). A real 2SIO
    // has no connection to the memory read strobe. It is not in that conversation.
    //
    // So the bus asks the SAME questions -- decodes(), assertsPhantom(), of every
    // board, in slot order -- and asks them ONCE, storing the answer. The board
    // still owns the entire decision. The bus still invents nothing. It just
    // stopped asking sixty-five thousand times a second.
    //
    // A board whose decode changes MUST say so (Board::decodeChanged()). If one
    // forgets, the tables go stale and the machine lies quietly -- so that is not
    // left to trust: setVerify(true) re-derives the decode the slow way on
    // every single cycle and screams if it disagrees with the table. The test
    // suite and the CPU validation gate both run with it on.
    void invalidateDecode() { dirty_ = true; }

    // Paranoid mode: re-derive the decode AND the interrupt wire the slow way, and
    // check them against what we cached -- on every cycle and every instruction.
    // Slower than the original code. That is fine: it is a proof, not a path.
    void setVerify(bool on) { verify_ = on; }
    bool verify() const { return verify_; }

    // The four cycles. Each is a two-pass affair:
    //   pass 1: ask every board whether it pulls PHANTOM* -> BusCycle::phantom
    //   pass 2: ask every board whether it decodes; exactly one should answer
    // Carrying a signal, then running a decode. No decisions.
    //
    // `status` is the 8080 STATUS WORD the originating master asserts for this cycle
    // (Status8080 above). The CPU generates it -- an opcode fetch is M1, a stack
    // access is STACK, and only the CPU knows which -- and the bus carries it verbatim
    // (settle() ORs the type-derived FALLBACK on, which is a subset, so a master word
    // is never corrupted). The default 0 means "no master annotated this cycle": the
    // monitor's memory pokes and a DMA transfer both leave it 0 and fall back to
    // statusFor() -- exactly the behavior before this
    // existed. 0 is safe as "unset" because a real memory-write's word is also 0x00
    // and the fallback reproduces it.
    //
    // memRead/memWrite take the master's 16-bit address. With a translator set
    // (setTranslator) that address is LOGICAL and goes through it first; with none --
    // always, except inside a mapped board's own step -- it is physical A0-A15, and the
    // cost of the check is one null-pointer test.
    //
    // KEEP THE TRANSLATED PATH OUT OF LINE (memReadMapped/memWriteMapped, in bus.cpp).
    // These two are inlined into every memory access of every CPU core's step(); a
    // translated path written here goes into each of those sites and slows every Altair
    // machine. The measurements: docs/devguide/ieee696-bus.md, "Speed".
    uint8_t memRead(uint16_t addr, uint8_t status = 0) {
        if (xlat_) [[unlikely]] return memReadMapped(addr, status);
        return memRead0(addr, status);
    }
    void memWrite(uint16_t addr, uint8_t data, uint8_t status = 0) {
        if (xlat_) [[unlikely]] {
            memWriteMapped(addr, data, status);
            return;
        }
        memWrite0(addr, data, status);
    }

    // I/O: `addr` is A0-A15 of the I/O cycle. A master that puts something on A8-A15
    // (the Z80's B or A, the 8080's port copy)
    // passes it; a caller that passes a plain port leaves A8-A15 at 0.
    uint8_t ioRead(uint16_t addr, uint8_t status = 0);
    void ioWrite(uint16_t addr, uint8_t data, uint8_t status = 0);

    // The physical address lines: A0-A23.
    static constexpr uint32_t addrMask() { return 0xFFFFFFu; }

    // ---- PHYSICAL memory cycles with A16-A23 ----
    //
    // `phys` is cut to addrMask() first.
    // Segment 0 (A16-A23 = 0) uses the same cached decode as memRead, and so does every
    // other segment unless some board decodes A16-A23 (Board::decodesExtended); then each
    // segment gets its own tables, built the first time a cycle reaches it and dropped
    // with the rest by invalidateDecode(). Never translated.
    uint8_t memReadX(uint32_t phys, uint8_t status = 0);
    void memWriteX(uint32_t phys, uint8_t data, uint8_t status = 0);
    uint8_t peekX(uint32_t phys) const;

    // ---- THE 16-BIT TRANSFER, sXTRQ*/SIXTN* (IEEE 696 2.6) ----
    //
    // The master asks for 16 bits at an EVEN physical address. If the board that decodes
    // the address answers SIXTN* (Board::answersSixtn), ONE cycle moves both bytes and the
    // result has sixtn = true. If not -- an 8-bit board, the floating bus, or an odd
    // address -- the result has sixtn = false
    // and NO cycle has run: nothing was driven, no board was strobed, no observer saw
    // anything. The master then does the byte-serial response itself (2.6.5.1), for
    // which byteSerialRead16/Write16 are the helper.
    Transfer16 memRead16(uint32_t phys, uint8_t status = 0);
    bool memWrite16(uint32_t phys, uint8_t even, uint8_t odd, uint8_t status = 0);

    // The byte-serial response: two ordinary byte cycles, at the even address and the odd
    // one after it, in the order the MASTER chooses. A word is at an even address (A0 = 0,
    // 2.6.4.2), so an odd `phys` is taken as the word that holds it: phys & ~1. IEEE 696 does not say which byte goes first; that is the
    // master's design (reference/IEEE_696_1983.md, 2.6.5.1).
    Transfer16 byteSerialRead16(uint32_t phys, bool evenFirst, uint8_t status = 0);
    void byteSerialWrite16(uint32_t phys, uint8_t even, uint8_t odd, bool evenFirst,
                           uint8_t status = 0);

    // ---- The address translator (see AddressTranslator). Null = none. ----
    void setTranslator(AddressTranslator* t) { xlat_ = t; }
    AddressTranslator* translator() const { return xlat_; }

    // An unvectored interrupt acknowledge floats to 0xFF, which the 8080 reads
    // as RST 7. Same floating-bus rule as unmapped memory -- not a special case.
    uint8_t intAck(uint8_t status = 0);

    // ---- pINT (pin 73) -- A WIRE, NOT A POLL ----
    //
    // A real bus does not poll a board for interrupt status. The board sets a signal
    // high or low on the bus, the CPU reads that signal off the bus, and the board
    // clears it again when its own design says to (Patrick, 2026-07-12).
    //
    // That was a description of the hardware. It was also a description of a bug, and
    // it was not written as one. This used to
    // walk the backplane and ask every card `assertsInt()` -- ONCE PER INSTRUCTION,
    // sixty million times a second, to compute a boolean that changes about a
    // thousand times a second on a busy machine. It cost more per instruction than
    // the entire rest of the bus once the decode was cached.
    //
    // Now a board PULLS the pin (Board::intChanged()) and the bus keeps the
    // wire-OR as a running count. Reading pin 73 is an integer test. The bus still
    // does exactly what it did before -- carry the OR of every board asserting it,
    // pick no winner, hand out no vector (DESIGN.md 4.4) -- it just stopped
    // conducting a survey to find out what was already on the wire.
    //
    // The vector, if there is one, comes from whoever claims the IntAck CYCLE --
    // like any other cycle. Nobody claims it in a machine with no VI card, so the
    // bus floats to 0xFF and the 8080 executes RST 7. That is not a fallback hack;
    // it is what the metal does, and it is why the PMMI's factory jumper straight
    // to pin 73 yields RST 7 with no vector logic anywhere.
    bool intPending() const;

    // A board's pin moved. Called by Board::intChanged(), and by nothing else.
    void intWireChanged(bool pulling) { intCount_ += pulling ? 1 : -1; }

    // ---- NMI* -- A WIRE, EXACTLY LIKE pin 73 ----
    //
    // Is any enabled board pulling NMI* right now? A level, read once per instruction
    // by a core that has the pin (the Z80); the 8080 and 8085 cores never read it.
    // Under setVerify(true) the wire is re-derived the slow way, as for pINT.
    bool nmiAsserted() const;

    // A board's NMI* moved. Called by Board::nmiChanged(), and by nothing else.
    void nmiWireChanged(bool pulling) { nmiCount_ += pulling ? 1 : -1; }

    // ---- pHOLD (pin 74) -- A WIRE, EXACTLY LIKE pin 73 ----
    //
    // A DMA card PULLS pHOLD and HOLDS it, and the run loop reads the wire at an
    // instruction boundary -- it does NOT interrogate every card per instruction
    // asking "do you want the bus?". That is the same poll §4.4.1 tore out for the
    // interrupt line, and it would be the same mistake: a bus does not survey its
    // cards, it reads what they have driven. So the bus keeps a running wire-OR count
    // (holdWireChanged(), from Board::holdChanged()) and the answer is one integer
    // test. Only when SOMEONE is pulling does the run loop walk the backplane to find
    // who -- and grant them by TMA priority, highest first (DESIGN.md 4.5).
    //
    // Read every instruction by the run loop's serviceDma(), so -- exactly like
    // intPending()/verifyInt() -- it is where setVerify(true) re-derives the wire the
    // slow way and aborts if a board changed its pHOLD and forgot to say so.
    bool holdPending() const;

    // A board's pHOLD moved. Called by Board::holdChanged(), and by nothing else.
    void holdWireChanged(bool pulling) { holdCount_ += pulling ? 1 : -1; }

    // ---- PRDY/XRDY (pins 72/3) -- READY, AND THE WAIT STATES IT COSTS ----
    //
    // A slow board pulls READY low in the cycle it is decoded, and the CPU idles in Tw
    // between T2 and T3 until the board lets go: the cycle finishes as normal, just
    // later (a D+7A holds it 5.5 us on every analog cycle so its A/D can convert). The
    // instruction then took more T-states than the core's table says, and the core
    // cannot know it -- only the board knows how long it held the line.
    //
    // So the board PUSHES the count from inside its own read()/write()
    // (Board::holdReady()), the bus sums it for the cycles in flight, and the run loop
    // takes the sum at the boundary and charges it to the Clock with the instruction
    // (DESIGN.md 7.5). Like pINT and pHOLD, nobody is asked: a board that never holds
    // READY -- all of RAM -- costs nothing. The bus carries the count and decides
    // nothing. Not serialized: it is zero at every instruction boundary.
    void addWaitStates(uint32_t n) { wait_ += n; }
    uint32_t takeWaitStates() { uint32_t w = wait_; wait_ = 0; return w; }

    // ---- VI0-VI7 (pins 4-11) -- EIGHT MORE WIRES, CARRIED AND NOT ARBITRATED ----
    //
    // The bus does for these exactly what it does for pin 73 and NOT ONE THING MORE:
    // it carries the wire-OR of every board pulling each line, and it hands out no
    // vector, picks no winner and applies no mask. Priority IS a chip -- an 8214, on
    // an 88-VI card -- and pretending the backplane knows about it would be the same
    // mistake §4.4 warns about with PHANTOM*.
    //
    // So an 88-VI is an ordinary board that happens to READ these wires, drive pin 73
    // itself, and claim the IntAck cycle. With no 88-VI in the machine the eight
    // lines go nowhere, which is what an empty slot does.
    uint8_t viLines() const;

    // A board's VI pins moved: it WAS pulling `before`, it is NOW pulling `after`.
    // Called by Board::intChanged(), and by nothing else.
    void viWireChanged(uint8_t before, uint8_t after) {
        uint8_t was = viMask_;
        for (int i = 0; i < 8; ++i) {
            uint8_t bit = (uint8_t)(1u << i);
            if ((before & bit) == (after & bit)) continue;
            viCount_[i] += (after & bit) ? 1 : -1;
        }
        recomputeVi();
        if (viMask_ != was) notifyViWatchers();
    }

    // ---- The cycle stream (DESIGN.md 3.0.3, 4.2.2) ----
    //
    // Every board already sees every cycle -- that is what a backplane IS. An
    // observer watches the SAME stream from outside the backplane, and that is
    // the whole implementation of BREAK IO, BREAK MEM, TRACE and HISTORY.
    //
    // So those are NOT CPU features and must never live in a core: they are
    // questions about bus cycles, they are answered here, and an 8085 or a Z80
    // inherits every one of them on the day it lands without writing a line.
    using Observer = std::function<void(const BusCycle&)>;
    int observe(Observer fn);   // returns a handle
    void unobserve(int handle);

    // ---- The pre-access veto (BREAK IO/MEM stops BEFORE the cycle) ----
    //
    // An Observer sees a cycle AFTER the device has driven it -- perfect for
    // HISTORY and TRACE, which record what happened, and it is where a DMA-driven
    // cycle breakpoint still stops. But a MEM/IO breakpoint on the CPU wants the
    // machine to stop with the PC on the instruction and NOTHING executed: no port
    // read, no byte written. That cannot be a post-access observer -- the access
    // already happened by then.
    //
    // So the debugger installs ONE veto, consulted at the TOP of every cycle,
    // before any board is asked. If it returns true the cycle function throws
    // CycleBreakBefore and the instruction unwinds untouched; the debugger catches
    // it and restores the CPU. Exactly one slot, like the machine's other single
    // wires -- the debugger is the only caller. A machine with no cycle breakpoint
    // never installs it and the four cycle paths pay one null-function test.
    using PreAccessVeto = std::function<bool(const BusCycle&)>;
    void setPreAccessVeto(PreAccessVeto fn) { preVeto_ = std::move(fn); }
    void clearPreAccessVeto() { preVeto_ = nullptr; }

    // Look without running a cycle -- for DISASM, TRACE and the debugger's
    // display, none of which are allowed to have side effects. Runs the SAME
    // decode (PHANTOM* and all), so a shadowed board is invisible to it exactly
    // as it is to a real read; it just never strobes anybody. Floats to 0xFF when
    // nobody can answer, which is what the bus would have done anyway.
    uint8_t peek(uint16_t addr) const;

    // `n` bytes of peek() from `addr` up (wrapping at 64K), in ONE call -- the same
    // bytes n peek()s would return. CPU HISTORY reads the opcode and its operands
    // before every instruction, and three separate trips through peek() were a
    // sixth of the run loop's time.
    void peekBytes(uint16_t addr, uint8_t* out, int n) const;

    // Reverse lookup: who answers here, and why. Backs WHO and the contention
    // detector. Returns every board that ACTUALLY decodes -- so a board that is
    // phantomed out does not appear, which is why a shadow is not contention.
    std::vector<Board*> respondersTo(const BusCycle& c) const;

    void setContentionPolicy(Contention p) { policy_ = p; }
    Contention contentionPolicy() const { return policy_; }

    // The unclaimed-I/O diagnostic (DESIGN.md 4.6.1, and enum Unclaimed above).
    void setUnclaimedPolicy(Unclaimed p) { unclaimedPolicy_ = p; }
    Unclaimed unclaimedPolicy() const { return unclaimedPolicy_; }

    // The CPU's current instruction address, so an unclaimed-port warning can name
    // the PC. The bus has no other way to know it -- published once per instruction
    // by the run loop (Debugger::run) BEFORE the instruction's cycles are driven.
    void setInstrPc(uint32_t pc) { instrPc_ = pc; }

    // The address the run loop last published -- read by the debug facility's
    // PC-prefix provider (dbg::setPcProvider), so every diagnostic line can name
    // the instruction that produced it.
    uint32_t instrPc() const { return instrPc_; }

    // Re-arm the once-per-(port,direction) de-dup. Called at the start of each
    // operator RUN/GO so an absent port is re-reported on a later run -- otherwise a
    // guest polling an absent UART thousands of times would bury the console.
    void resetUnclaimedWarnings() {
        warnedIoRead_.reset();
        warnedIoWrite_.reset();
        unclaimedHalt_ = false;
        busHang_ = false;  // also the start of a run: a stale request must not stop it
    }

    // A BOARD THAT WOULD HANG THE REAL BUS (StopReason::BusHang, DESIGN.md 3.0.4). A board
    // whose real counterpart holds READY forever for this cycle (the SPUZ, accessed while its
    // Z80 is in reset) lets the cycle float, logs why, and calls this. The run loop stops at
    // the instruction boundary, as for SET BUS UNCLAIMED=HALT.
    void requestBusHang() { busHang_ = true; }
    bool takeBusHang() {
        bool h = busHang_;
        busHang_ = false;
        return h;
    }

    // Read-and-clear the pending Halt request: Unclaimed::Halt set it when a guest
    // hit an unclaimed I/O port. The run loop checks this at the instruction
    // boundary, exactly as it checks a cycle breakpoint.
    bool takeUnclaimedHalt() {
        bool h = unclaimedHalt_;
        unclaimedHalt_ = false;
        return h;
    }
    // Which access tripped the last Halt -- for the stop message.
    uint8_t haltPort() const { return haltPort_; }
    bool haltWasWrite() const { return haltWrite_; }

    // Every message the bus has emitted (contention, discarded writes). The
    // monitor prints these; MCP returns them as structured data.
    const std::vector<std::string>& drain() const { return log_; }
    void clearLog() { log_.clear(); }

    // The last cycle answered nobody. DEPOSIT uses this to say "byte discarded"
    // out loud instead of silently doing nothing -- silence here is a bug that
    // takes hours to find.
    bool lastUnclaimed() const { return unclaimed_; }

    // More than one board answered the last cycle. Contention is a BUS fact (unlike
    // origin, which is not -- see BusCycle) and already computed on every cycle; the
    // observer reads it so TRACE's CONTENTION mask and HISTORY can flag the cycle.
    bool lastContended() const { return contended_; }

    // WHO answered the last cycle -- the first board in slot order that drove it, or
    // null when nobody did (the floating bus). This is NOT a new question: the cycle
    // already resolved the winning decoder (Slot::who, or Decode::first) to move the
    // byte, and this just keeps the pointer it already had instead of throwing it
    // away. The observer reads it to record HISTORY's "who responded" column -- so it
    // costs one pointer store per cycle and never a re-scan (contrast respondersTo(),
    // which allocates). On contention it is the first driver; lastContended() flags
    // the rest. Valid only immediately after a cycle, like lastContended().
    Board* lastResponder() const { return responder_; }

    // PIN 49, CLOCK: 2 MHz of real time, from every CPU board (core/busclock.h). A
    // signal like the others here, so the bus carries it; a board that divides it reads
    // the count from here and arms its own deadline.
    BusClock&       busClock() { return busClock_; }
    const BusClock& busClock() const { return busClock_; }

private:
    BusClock busClock_;

    bool anyAssertsPhantom(const BusCycle& c) const;

    // THE SAME QUESTION AS decoders(), WITHOUT THE VECTOR.
    //
    // decoders() returns its answer by value, so asking it costs a heap
    // allocation -- and the cycle functions ask it on EVERY guest read and EVERY
    // guest write. That malloc/free pair was measured at two thirds of the cost of
    // a memory access (72ns -> 26ns when removed), which is to say: most of the
    // time this simulator spent was spent allocating a vector to hold the number 1.
    //
    // Nothing about the MODEL changes here. Every board is still asked, in slot
    // order, and the board still owns the whole decision. We just stopped putting
    // the answer on the heap.
    //
    // One decoder is the overwhelming case, so that is the case with no allocation
    // at all. Contention is rare, already a fault, and about to print a line of
    // text -- it can afford decoders().
    struct Decode {
        Board* first = nullptr;  // the first board in slot order that drives
        int n = 0;               // how many drive. >1 is contention.
    };
    Decode scan(const BusCycle& c) const;
    // An I/O read that must take the exact path: more than one decoder, or one that drives
    // only some data lines (Board::drivesBits), or a port shared by design
    // (Board::sharesPort). The cache and the verifier share this test.
    bool exactRead(const BusCycle& c, const Decode& d) const;
    // An I/O write that every decoder shares by design: not contention (DESIGN.md 4.6).
    bool sharedWrite(const BusCycle& c, const std::vector<Board*>& who) const;

    std::vector<Board*> decoders(const BusCycle& c) const;
    void reportContention(const BusCycle& c, const std::vector<Board*>& who);

    // The cycle fast paths behind memRead/memWrite (segment 0, physical). Out of line;
    // the inline wrappers above add only the translator test.
    uint8_t memRead0(uint16_t addr, uint8_t status);
    void memWrite0(uint16_t addr, uint8_t data, uint8_t status);

    // The translated path behind memRead/memWrite: the logical address through the
    // translator, the physical cycle, and (a read) the translator's last word on the data
    // (jump-on-reset). Only for xlat_ set. Defined in bus.cpp, so that they are not inlined:
    // see memRead. (The build has no link-time optimization. If it gets it, measure again.)
    uint8_t memReadMapped(uint16_t addr, uint8_t status);
    void memWriteMapped(uint16_t addr, uint8_t data, uint8_t status);

    // An I/O cycle no board decoded, under a non-Silent Unclaimed policy. Formats
    // one line into log_ (de-duped per port+direction) and, under Halt, arms the
    // boundary stop. Called only from the four I/O paths -- never from memory.
    void reportUnclaimed(const BusCycle& c);

    // ---- The cached decode ----
    //
    // One entry per 256-byte page (memory) or per port (I/O), per cycle class.
    // Four tables, because a card is wired to sMEMR, sINP and sOUT separately and
    // a ROM region famously does not decode a WRITE at all -- so "who answers
    // here" has a different answer for a read than for a write, and that fell out
    // of the model rather than being bolted onto it.
    struct Slot {
        Board* who = nullptr;   // the single board that drives. null: nobody -> floats 0xFF
        bool phantom = false;   // PHANTOM* as resolved for this page and this cycle class
        bool slow = false;      // more than one driver, or a partial one: take the exact path.
    };

    // MEMORY DECODE IS PAGE-GRANULAR (256 bytes), and that is a CONTRACT on
    // Board::decodes(), written down in board.h. It is not a guess: real S-100
    // memory decoding is done from the high address lines at 1K granularity at the
    // very finest, and our own MemoryBoard is built on a 256-entry page map
    // already. I/O needs no such contract -- all 256 ports are stored exactly.
    Slot memRead_[256], memWrite_[256];
    Slot ioRead_[256], ioWrite_[256];

    // IEEE 696: the same two tables for each other 64K segment (A16-A23 = 1..255) -- only
    // when some board decodes A16-A23; otherwise every segment shares segment 0's tables.
    // Built the first time a cycle reaches the segment and dropped by rebuild(). Index 0 is
    // never used -- segment 0 is memRead_/memWrite_ above, so the Altair path is unchanged.
    struct SegTables {
        Slot rd[256], wr[256];
    };
    std::unique_ptr<SegTables> seg_[256];
    SegTables& segTables(uint8_t ext);

    // The shared body of a cached memory cycle once the slot is chosen (segment 0 or not).
    uint8_t readSlot(const Slot& s, uint16_t addr, uint8_t ext, uint8_t status);
    void writeSlot(const Slot& s, uint16_t addr, uint8_t ext, uint8_t data, uint8_t status);

    Slot resolve(Cycle t, uint16_t addr, uint8_t ext = 0) const;
    // Fill one segment's two page tables (the per-page loop of rebuild(), probe and all).
    void buildPages(uint8_t ext, Slot* rd, Slot* wr) const;
    void rebuild();
    void verifySlot(const BusCycle& c, const Slot& s) const;
    void verifyInt() const;
    void verifyNmi() const;
    void verifyHold() const;

    // pINT as a WIRE-OR: how many enabled boards are pulling it down right now.
    // Maintained by intWireChanged(); never recomputed on the hot path.
    int intCount_ = 0;

    // NMI* as a WIRE-OR, the same way. Maintained by nmiWireChanged().
    int nmiCount_ = 0;

    // pHOLD as a WIRE-OR: how many enabled boards want the bus right now. Maintained
    // by holdWireChanged(); read once per instruction as holdPending(), so a machine
    // with no DMA card in it pays a single integer test and never a per-board poll.
    int holdCount_ = 0;

    // READY wait states pushed by the boards for the cycles of the instruction in
    // flight (addWaitStates()); the run loop drains it at the boundary.
    uint32_t wait_ = 0;

    // The same, eight times over, for VI0-VI7 -- plus the bitmask an 88-VI actually
    // reads, kept in step so that viLines() is a load and not a loop.
    int     viCount_[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t viMask_     = 0;
    void    recomputeVi() {
        uint8_t m = 0;
        for (int i = 0; i < 8; ++i)
            if (viCount_[i] > 0) m |= (uint8_t)(1u << i);
        viMask_ = m;
    }
    void verifyVi() const;

    // A VI line moved, so every card WATCHING those lines has a pin 73 that may have
    // moved with it. The guard is not paranoia: a watcher's intChanged() can itself
    // land back here (an 88-VI whose own RTC sits on a VI line), and one bounce is
    // enough -- its VI output does not depend on the VI inputs, so it settles.
    // (Out of line: Board is only forward-declared up here.)
    bool inViNotify_ = false;
    void notifyViWatchers();

    // The exact path. THIS IS THE DEFINITION OF CORRECTNESS; the tables above are
    // a cache OF it, and the verifier checks them AGAINST it.
    uint8_t memReadExact(uint16_t addr, uint8_t ext, uint8_t status);
    void memWriteExact(uint16_t addr, uint8_t ext, uint8_t data, uint8_t status);
    uint8_t ioReadExact(uint16_t addr, uint8_t status);
    // The S-100 half of ioRead/ioWrite, after the translator and the veto.
    uint8_t ioReadBus(uint16_t addr, uint8_t status);
    void ioWriteBus(uint16_t addr, uint8_t data, uint8_t status);
    void ioWriteExact(uint16_t addr, uint8_t data, uint8_t status);

    // The memory cycle in any segment, and the slot it uses (bus.cpp).
    const Slot& pageSlot(Cycle t, uint8_t ext, uint16_t addr);
    uint8_t readAt(uint16_t addr, uint8_t ext, uint8_t status);
    void writeAt(uint16_t addr, uint8_t ext, uint8_t data, uint8_t status);
    // Some board decodes A16-A23 (Board::decodesExtended), so segments need their own
    // tables. Set by rebuild().
    bool extDecode_ = false;

    // Who decodes this memory cycle, for a 16-bit request: the cached answer, or the exact
    // scan on a slow page (null unless exactly one board drives). Sets c.phantom.
    Board* decoderFor16(BusCycle& c);

    bool dirty_ = true;
    bool verify_ = false;
    AddressTranslator* xlat_ = nullptr;

    // Boards that WATCH cycles they do not answer. Almost none do -- the Tarbell
    // does -- so calling snoop() on every card on every cycle was N virtual calls
    // to reach a do-nothing default. They opt in with Board::wantsSnoop().
    std::vector<Board*> snoopers_;

    // Third pass, once per cycle: show the completed cycle to every board.
    // The bus is not notifying anyone -- the cycle was on the backplane the whole
    // time and they could all see it. This is only where we let them LATCH it.
    // Takes c by NON-const ref because this is also where the 8080 status word is
    // latched onto it (BusCycle::status), alongside the `data` back-fill.
    void settle(BusCycle& c);

    std::vector<Board*> boards_;
    std::vector<std::string> log_;
    Contention policy_ = Contention::Warn;
    bool unclaimed_ = false;
    bool localIo_ = false;   // the current I/O cycle is the master card's own (ownsIo)
    bool contended_ = false;
    Board* responder_ = nullptr;  // who drove the last cycle -- see lastResponder()

    // The unclaimed-I/O diagnostic (DESIGN.md 4.6.1). Default Silent -- opt-in.
    Unclaimed unclaimedPolicy_ = Unclaimed::Silent;
    uint32_t instrPc_ = 0;                // the running instruction's PC, for the message
    std::bitset<256> warnedIoRead_;       // ports already warned this run: IN ...
    std::bitset<256> warnedIoWrite_;      // ... and OUT, kept apart -- the message names it
    bool unclaimedHalt_ = false;          // Halt tripped; the run loop stops at the boundary
    bool busHang_ = false;                // a board asked for a BusHang stop (requestBusHang)
    uint8_t haltPort_ = 0;                // which port, and ...
    bool haltWrite_ = false;              // ... which direction, for the stop message

    std::vector<std::pair<int, Observer>> observers_;
    int nextObserver_ = 1;

    // The pre-access veto -- null unless the debugger has installed one for a run
    // that carries a cycle breakpoint. See setPreAccessVeto.
    PreAccessVeto preVeto_;
};

// Sets the translator for one scope and puts the old one back when the scope ends --
// also when a CycleBreakBefore unwinds out of the core's step, which a hand-written
// setTranslator(nullptr) after the step would miss: the monitor and DMA would then be
// translated through the CPU's map.
class TranslatorScope {
public:
    TranslatorScope(Bus& bus, AddressTranslator* t) : bus_(bus), was_(bus.translator()) {
        bus_.setTranslator(t);
    }
    ~TranslatorScope() { bus_.setTranslator(was_); }
    TranslatorScope(const TranslatorScope&) = delete;
    TranslatorScope& operator=(const TranslatorScope&) = delete;

private:
    Bus& bus_;
    AddressTranslator* was_;
};

} // namespace altair
