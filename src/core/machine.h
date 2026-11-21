#pragma once
//
// Machine -- the backplane, plus the cards in it.
//
// The monitor and the MCP server both sit on THIS and on Board::properties(),
// which is why they cannot drift (DESIGN.md 11). There is no "CLI model" and no
// "MCP model"; there is a machine, and two ways to talk to it.

#include "core/board.h"
#include "core/bus.h"
#include "core/debug.h"
#include "core/symbols.h"
#include "cpu/cpu.h"

#include <memory>
#include <string>
#include <vector>

namespace altair {

class Machine {
public:
    std::string name = "altair";

    // THERE IS NO MACHINE CLOCK, and there must never be one again. The crystal is
    // on the CPU card, so `clock_hz` is that BOARD's property (DESIGN.md 3, 8).
    // A machine-level copy would be a second place to say one thing, and the day
    // the two disagreed the machine would run at whichever was written last.

    // AND THERE IS NO MACHINE-LEVEL `sense` EITHER, for exactly the reason above.
    // It was here once: a byte, parsed from `[machine] sense`, printed by SHOW
    // MACHINE -- and connected to nothing, because no board decoded port 0xFF. The
    // guest's `IN 0FFH` read the floating bus and got 0xFF whatever the operator
    // configured, which meant the DBL boot PROM (it tests bit 4 to pick the 2SIO's
    // stop bits) was reading a wire, not a switch.
    //
    // The switches are on the front panel, the front panel is a card, and the card
    // is `fp` (boards/mits-frontpanel.h). That is where the byte lives now.

    // Monitor commands run once the backplane exists. This is why there is no
    // BOOT command (DESIGN.md 10.0): a config that should boot says
    // startup = ["GO FF00"], which is the operator's keystroke, written down.
    std::vector<std::string> startup;

    // THE DIRECTORY THIS MACHINE CAME OUT OF -- and "" if it came out of the binary.
    //
    // A machine file's relative paths are relative to THAT FILE (core/paths.h), and
    // `startup` is a list of commands that came out of that file, so the monitor has
    // to know where it was in order to run them. This is the only thing that carries
    // the answer from the loader to Monitor::runStartup().
    //
    // Empty for a built-in, which is right and not a special case: a built-in machine
    // lives in .rodata and is in no directory at all, so every path it names can only
    // be relative to the shell -- which is exactly what "" means everywhere else.
    //
    // But empty does NOT mean "built-in": a file NAMED IN THE CWD (`pascal100 foo.toml`
    // from foo.toml's own folder) has an empty dirname too, and its paths correctly
    // resolve against the shell just the same. So `dir.empty()` cannot tell a built-in
    // from a cwd-local file -- an empty dirname and no file are different answers that
    // share one value (the read()==0 shape, issue #25). Whether a file was loaded is
    // `fromFile`, below; do not infer origin from `dir`.
    std::string dir;

    // TRUE if this machine was loaded from a file, FALSE if it came out of the binary.
    // The only reliable answer to "is this built in?" -- decided by SPELLING at load
    // time (looksLikeFile(), config/toml.cpp), never by whether `dir` happens to be
    // empty. SHOW PATHS uses this, and nothing else should conflate it with `dir`.
    bool fromFile = false;

    Bus bus;

    // Emulated time (DESIGN.md 7.5). Advanced ONLY by the run loop, by exactly
    // the T-states the CPU reported retiring -- so a board's sense of time and
    // the guest's are the same sense of time, and a session replays identically.
    Clock clock;

    // Give every board's host endpoints a turn: drain a keyboard, accept a
    // socket. Once per time slice, never inside a bus cycle (DESIGN.md 7.7).
    void pump();

    // BYTES THE GUEST HAS RECEIVED ACROSS THE WHOLE BACKPLANE, monotonic. The run loop
    // watches its delta to know whether a byte is arriving anywhere -- the one signal that
    // tells a transfer from a prompt, on ANY line and not just the console (Board::rxBytes,
    // monitor.cpp). Summed on demand: a handful of boards, once per slice, is noise.
    uint64_t rxBytes() const;

    Board* find(const std::string& id);
    Board* add(const std::string& type, const std::string& id, std::string& err);

    // Fit a board that is already built -- into the backplane, onto the bus and the
    // clock -- exactly as add() does once makeBoard() has built it. For a board no
    // registry knows: a test's own fake. The caller sets its id and keeps it unique.
    Board* adopt(std::unique_ptr<Board> b);
    bool remove(const std::string& id, std::string& err);
    const std::vector<std::unique_ptr<Board>>& boards() const { return boards_; }

    // FIT THE CARDS FROM `built` INTO THIS BACKPLANE, and throw away whatever was in
    // it. `built` is left empty. This is how a machine FILE is loaded (config/toml.h),
    // and the indirection is the entire point:
    //
    // A LOAD EITHER HAPPENS OR IT DOES NOT. The file is built into a SCRATCH machine
    // first, so a file that dies halfway -- a key that does not parse, a disk image
    // that is not there, an id it uses twice -- takes the scratch machine down with it
    // and never touches this one. It used to be neither: CONFIG LOAD merged straight
    // into the live backplane, so a failure left behind whichever cards it had already
    // fitted and the error handed you a machine that was neither the one you had nor
    // the one you asked for.
    //
    // A CARD IS MOVED, NOT REBUILT. It keeps the disk you mounted in it and the socket
    // it opened -- the load did that work once and it is not done twice. What it does
    // NOT keep is the backplane it was built in: it is re-attached to THIS bus and THIS
    // clock, because the ones it was fitted to belong to a scratch Machine that is
    // about to cease to exist. A board holds a bare `Clock*` (board.h) and would be
    // reading freed memory the moment the run loop turned.
    //
    // It does not POWER the machine. That is the caller's, and both roads take it: the
    // command line powers after loading (main.cpp) and so does CONFIG LOAD.
    void replaceWith(Machine& built);

    // EMPTY THE BACKPLANE, exactly as `-n` on the command line does (main.cpp) -- every
    // card off the bus, no memory, nothing driving anything. This is where you START if
    // you are building a machine up with BOARDS ADD, and it is what the monitor's `MACHINE
    // none` runs so a `DO` script can build its own machine from scratch. Each card is
    // detached through the same door replaceWith() uses (bus.detach), never just dropped:
    // its pins -- interrupt, pHOLD, VI, the decode it answered -- are on the bus, not in
    // the destructor. Like replaceWith(), it does NOT power the empty result: an empty
    // machine has nothing to power, and the caller powers once it has fitted its cards.
    void clear();

    // THE PROM BURNER (DESIGN.md 10.2). Put a byte where a bus cycle cannot: into a
    // ROM. A ROM region does not decode a write (§4.2) because on real hardware a bus
    // write cannot program a PROM either -- you pull the chip and put it in a
    // programmer, and that is not a bus operation. This is the programmer.
    //
    // It lives HERE, not in the monitor, for the reason hex.h gives: one implementation,
    // more than one front end. The monitor's `LOAD ... ROM` and MCP's `mem_load {rom:
    // true}` are the same operation, and two copies of it would be two copies that
    // drift.
    //
    // The address is a BUS address, 0000-FFFF -- the same address everything else in
    // this program means (Patrick, 2026-07-17: every address refers to the one 64K
    // address space). Which chip it belongs to is a question the address already
    // answers, so nobody names a board. To reach a bank that is not selected, select it
    // (SET mem0 bank=3); the guest has to, and so do you.
    //
    // False means the byte did NOT land, and `why` says which of the two reasons it was
    // -- nobody home, or more than one board answering. They are different bugs, and a
    // burner that silently drops a byte is the bug this monitor exists not to have.
    // `phys` is a physical address: A0-A23.
    bool burn(uint32_t phys, uint8_t v, std::string& why);

    // IEEE 696: the 64K segments (A16-A23) where board `b` decodes its own memory ranges,
    // for the memory map and the contention walk (monitor and MCP). Empty for a board that
    // does not decode A16-A23: that board answers alike in every
    // segment (Board::decodesExtended(), board.h), so segment 0 shows all of it.
    std::vector<uint32_t> segmentsOf(const Board& b) const;

    // The segments a contention walk must cover (SHOW BUS CONTENTION, MCP bus_contention):
    // segment 0 and every segmentsOf() of every board, sorted. Any other segment holds only
    // the boards that answer everywhere, the same as segment 0 without the segment boards,
    // so it can add no contention.
    std::vector<uint32_t> contentionSegments() const;

    void reset(Reset r);

    // Power applied. THE ONLY THING THAT LOSES RAM (DESIGN.md 6).
    void power();

    // ---- SNAPSHOT / RESTORE (DESIGN.md 13) ----
    //
    // Write the whole machine's STATE to a file, and read it back. A snapshot is CPU
    // + Clock time + every board's serialize() -- NOT its config. So RESTORE loads
    // into a machine ALREADY BUILT from a config (a built-in, or a CONFIG LOAD). Config
    // is a machine (CONFIG LOAD); a snapshot is its state, and the two stay separate.
    //
    // It lives here, not in the monitor, for the reason burn() does: one
    // implementation, more than one front end (the monitor now, MCP later).
    //
    // RESTORE IMPORTS WHAT IT CAN, so a snapshot moves to a newer release. Boards are
    // matched by id and type; each board it cannot restore -- one on only one side, one
    // whose state is from an older format (stateSince in machine.cpp), one whose section
    // does not read back exactly -- starts as at power-on, and `notes` gets a line
    // naming it. False + `err`, with the machine untouched, only for a file that is
    // unreadable (checksum, magic, truncated), from a NEWER format, or that shares no
    // board with this machine.
    bool snapshot(const std::string& path, std::string& err) const;
    bool restore(const std::string& path, std::string& err, std::vector<std::string>& notes);

    // ---- Who is driving? (DESIGN.md 3) ----
    //
    // Both of these ASK THE BACKPLANE and are allowed to answer "nobody". A
    // machine with no CPU card in it is a real machine you can build -- it is the
    // one milestone 1a ran, with the monitor as bus master -- so every caller has
    // to cope with a null, and the compiler makes sure they do.
    //
    // They are also recomputed on every call rather than cached. That is what lets
    // BOARDS REMOVE cpu0 work while the monitor is sitting there, and what will let
    // a dual-processor card switch cores under the debugger's feet without the
    // debugger noticing anything unusual.
    BusMaster* master();
    CpuCore* cpu();

    // The CPU CARD, not just its running core -- where the crystal is, and where the
    // run loop reports back the crystal it actually achieved (CpuCard::achievedHz).
    // Null on a backplane with no processor, exactly like cpu().
    CpuCard* cpuCard();

    // A CORE BY NAME: "<board>:<unit>", e.g. "p100:me" -- the board:unit form MOUNT uses
    // (DESIGN.md 3.0.1: cores are units). Case does not matter. Null when there is no such
    // board, it is not a CPU board, or it has no core by that name. `canonical`, when given,
    // gets the name as the machine spells it ("p100:me"), for listings.
    // It also finds a processor on a board's own bus (Board::privateCpus): "spuz0:z80".
    CpuCore* coreByRef(const std::string& ref, std::string* canonical = nullptr);

    // A PROCESSOR ON A BOARD'S OWN BUS (DESIGN.md 3.0.4), by name or by core, with the
    // board that owns it. Null when there is none: the core is the host's (a CpuCard's).
    PrivateCpu* privateByRef(const std::string& ref, Board** owner = nullptr,
                             std::string* canonical = nullptr);
    PrivateCpu* privateOf(const CpuCore* c, Board** owner = nullptr);
    // The byte at `a` as core `c` fetches it, without a bus cycle: through a private core's
    // own bus and bank (PrivateCpu::peekCode), through the card's map for a host core
    // (CpuCard::peekCodeByte), else straight off the bus. REGS, DISASM and DUMP read with it.
    uint8_t peekCode(CpuCore* c, uint32_t a);
    // Every private processor in the machine, with its board.
    std::vector<std::pair<Board*, PrivateCpu*>> privateCpus();

    // The unit name of core `c` ("me", "z80") -- but ONLY on a board with more than one
    // core; empty otherwise. What REGS, STEP, HISTORY CPU and MCP put in front of a
    // register line, so a one-core machine prints exactly what it always printed.
    std::string coreUnit(const CpuCore* c);
    // ...as the prefix of a register line: "[me] ", or "" on a one-core board. A private
    // processor is always named in full: "[spuz0:z80] ".
    std::string coreTag(const CpuCore* c);

    // The instruction set the machine currently speaks -- the active core's own
    // answer. Empty when there is no CPU, which is why DISASM in a CPU-less
    // machine asks you to say CPU=8080 rather than guessing (DESIGN.md 3.0.2).
    std::string isa();

    // Two boards both claiming to drive the bus is contention, and we say so
    // rather than picking one. Same rule as two boards decoding one address.
    std::vector<Board*> masters();

    // IEEE 696 TMA arbitration (2.8.3): can the temporary masters on this backplane be
    // arbitrated? False, with the reason in `why`, when two enabled DMA boards have the
    // same TMA priority, or when a DMA board with no TMA lines (tmaPriority() == -1)
    // shares the backplane with another DMA board. On a real bus both cards would
    // believe they had won. The TOML loader refuses such a file, and the run loop
    // refuses to start (StopReason::TmaConflict), because a board can be added or a
    // switch set after the load. One or no DMA board is always fine.
    // It also asks each board for a backplane rule of its own (Board::backplaneConflict),
    // refused the same way: the SPUZ refuses a second board with its select.
    bool tmaConflict(std::string& why);

    // True only while the debugger's run loop is turning. This is a DIFFERENT thing
    // from setRunning() above: this flag is false whenever pump() runs (pump() is
    // called between slices, not during one), so it cannot drive a run/stop lamp --
    // that is exactly why setRunning() exists as a separate operator-level signal.
    //
    // IT DOES NOT GATE PROPERTIES, and this comment used to say it did. There is no
    // "config-time only" property and there is no `runtime` flag -- every property can be
    // set, always (board.h, above setProperty(); Patrick, 2026-07-12). The rule was deleted
    // and this comment was left behind describing it, which is the most expensive kind of
    // stale comment: it documents a safety check that a reader will then go looking for,
    // fail to find, and "restore". Corrected 2026-07-14.
    bool running = false;

    Debugger debug{*this};

    // Operator-loaded symbols (.PRN / .SYM). HOST-SIDE STATE, exactly like the debugger's
    // breakpoints, and it behaves like them at every seam: it survives RESET and POWER, and
    // replaceWith() leaves it alone -- so a CONFIG LOAD keeps it, the same way a breakpoint
    // survives one (both are the operator's view, not the machine's). SYMBOLS CLEAR is its
    // NOBREAK. A machine's own `startup` may SYMBOLS LOAD more, which merge in.
    //
    // It lives at machine level, not on a board, because it belongs to no card -- it is a
    // view OF the 64K address space, not a property IN it, so the "no machine-level board
    // state" rule above does not reach it (DESIGN.md 10.3.2). See core/symbols.h.
    SymbolTable syms;

    // Collected board chatter (bad bank selects, ROM load failures), drained by
    // whichever front end is listening.
    std::vector<std::string> drainBoardLog();

private:
    std::vector<std::unique_ptr<Board>> boards_;
    uint64_t titleEpochSeen_ = 0;  // ByteStream::titleEpoch() at the last title walk (pump)
};

} // namespace altair
