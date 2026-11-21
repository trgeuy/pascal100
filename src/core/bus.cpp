#include "core/bus.h"

#include "core/board.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace altair {

void Bus::attach(Board* b) {
    boards_.push_back(b);
    b->attachBus(this);
    // It may ALREADY be pulling pin 73 -- a card does not stop asking to be
    // serviced because you moved it to another slot, and a card built on the bench
    // arrives with its pins in whatever state it left them in. (intWire() already
    // accounts for enabled: a disabled card drives nothing, and says so when it is
    // disabled, not when it is asked.)
    if (b->intWire()) ++intCount_;
    if (b->nmiWire()) ++nmiCount_;    // ...and it may arrive already pulling NMI*
    if (b->holdWire()) ++holdCount_;  // ...and it may arrive already pulling pHOLD
    viWireChanged(0, b->viWire());  // ...and whatever VI lines it arrived pulling
    // If the card offers debug flags, give it a diagnostic channel named by its id
    // (core/debuglog.h). Idempotent and not undone by detach(), so moving the card
    // between backplanes keeps the operator's DEBUG= selection. `id` is set by now.
    b->ensureDebugChannel();
    invalidateDecode();  // a card went into a slot: the wiring changed
}

void Bus::detach(Board* b) {
    boards_.erase(std::remove(boards_.begin(), boards_.end(), b), boards_.end());
    if (b->intWire()) --intCount_;  // and it takes its interrupt out with it
    if (b->nmiWire()) --nmiCount_;  // ...and its NMI* with it
    if (b->holdWire()) --holdCount_;  // ...and its pHOLD request comes out too
    viWireChanged(b->viWire(), 0);  // ...and its VI lines go with it
    b->attachBus(nullptr);
    invalidateDecode();  // ...and the wiring changed again when it came out
}

bool Bus::anyAssertsPhantom(const BusCycle& c) const {
    for (Board* b : boards_)
        if (b->enabled() && b->assertsPhantom(c)) return true;
    return false;
}

std::vector<Board*> Bus::decoders(const BusCycle& c) const {
    std::vector<Board*> out;
    for (Board* b : boards_)
        if (b->enabled() && b->decodes(c)) out.push_back(b);
    return out;
}

Bus::Decode Bus::scan(const BusCycle& c) const {
    Decode d;
    for (Board* b : boards_)
        if (b->enabled() && b->decodes(c)) {
            if (!d.first) d.first = b;
            ++d.n;
        }
    return d;
}

std::vector<Board*> Bus::respondersTo(const BusCycle& in) const {
    BusCycle c = in;
    c.phantom = anyAssertsPhantom(c);
    return decoders(c);
}

uint8_t Bus::peek(uint16_t addr) const {
    // peek is on the instruction flight recorder's hot path -- debug.cpp snapshots
    // three opcode bytes with it for EVERY executed instruction -- so it takes the
    // same cached decode the live read path (memRead) uses instead of re-scanning
    // the backplane. memRead_[page] names the single board that answers a MemRead in
    // that 256-byte page; on a non-slow page that IS the only decoder (slow == n>1),
    // so peeking it is identical to the exact first-responder scan, minus the scan.
    // The phantom overlay is already baked into who: resolve() ran anyAssertsPhantom
    // before deciding, and peek() itself takes no cycle, so there is nothing to pass.
    if (!dirty_) {
        const Slot& s = memRead_[addr >> 8];
        // A slow page has no cached answer to check -- the same order as memRead().
        if (!s.slow) {
            if (verify_) verifySlot(BusCycle{Cycle::MemRead, addr, 0, false}, s);
            uint8_t v = 0xFF;
            if (s.who) s.who->peek(addr, v);  // false leaves 0xFF: the floating bus
            return v;
        }
        // slow page (contention, or a sub-page decoder like the Tarbell): fall through
    }

    // No cache to trust -- dirty (nothing has rebuilt it yet this run) or a slow page.
    // The exact first-responder scan, still without building a decoders() vector.
    BusCycle c{Cycle::MemRead, addr, 0, false};
    c.phantom = anyAssertsPhantom(c);
    for (Board* b : boards_)
        if (b->enabled() && b->decodes(c)) {
            uint8_t v = 0xFF;
            if (b->peek(addr, v)) return v;
        }
    return 0xFF;  // nobody could answer without side effects. Neither can we.
}

void Bus::peekBytes(uint16_t addr, uint8_t* out, int n) const {
    for (int i = 0; i < n; ++i) {
        uint16_t a = (uint16_t)(addr + i);
        // Anything but a trusted, single-driver page -- a dirty cache, a slow page, or
        // verify mode wanting to prove the slot -- is peek()'s own business.
        const Slot& s = memRead_[a >> 8];
        if (dirty_ || verify_ || s.slow) {
            out[i] = peek(a);
            continue;
        }
        uint8_t v = 0xFF;
        if (s.who) s.who->peek(a, v);  // exactly peek()'s fast path
        out[i] = v;
    }
}

static const char* cycleName(Cycle t) {
    switch (t) {
    case Cycle::MemRead: return "read";
    case Cycle::MemWrite: return "write";
    case Cycle::IoRead: return "IN";
    case Cycle::IoWrite: return "OUT";
    case Cycle::IntAck: return "INTA";
    }
    return "?";
}

void Bus::reportContention(const BusCycle& c, const std::vector<Board*>& who) {
    if (policy_ == Contention::Silent) return;

    char buf[160];
    bool io = (c.type == Cycle::IoRead || c.type == Cycle::IoWrite);
    if (io)
        std::snprintf(buf, sizeof buf, "CONTENTION: port 0x%02X (%s) driven by", c.port(),
                      cycleName(c.type));
    else if (c.ext)  // IEEE 696, above 64K: the full 24-bit address
        std::snprintf(buf, sizeof buf, "CONTENTION: 0x%06X (%s) driven by", (unsigned)c.phys(),
                      cycleName(c.type));
    else
        std::snprintf(buf, sizeof buf, "CONTENTION: 0x%04X (%s) driven by", c.addr,
                      cycleName(c.type));

    std::string m = buf;
    for (Board* b : who) m += " " + b->id;
    // Two boards both actually driving is a real electrical fault, and a real
    // backplane would hand you exactly this bug. We report it; we do not pick a
    // winner. Picking a winner is how a simulator lies to you.
    m += "  -- both boards drive. The bus does not arbitrate (DESIGN.md 4.6).";
    log_.push_back(m);
}

// The floating-bus diagnostic (DESIGN.md 4.6.1). A guest that reaches for a board
// that isn't there reads 0xFF for ever and hangs -- this is the line that says why.
// Called from the four I/O paths only; memory is deliberately never warned. The PC
// is whatever the run loop last published (setInstrPc); on a bare interactive I/O
// with no run in flight it is simply the last instruction the CPU retired.
void Bus::reportUnclaimed(const BusCycle& c) {
    // The caller has already checked the policy is not Silent, but keep the guard so
    // the function is safe to call unconditionally too.
    if (unclaimedPolicy_ == Unclaimed::Silent) return;

    const bool write = c.isWrite();
    std::bitset<256>& seen = write ? warnedIoWrite_ : warnedIoRead_;
    const uint8_t port = c.port();
    if (seen.test(port)) return;  // once per port+direction per run -- see resetUnclaimedWarnings()
    seen.set(port);

    char buf[160];
    if (write)
        // e.g. warning: OUT 0FE <- 01 at PC=0113: no board decodes port 0xFE. reads float to 0xFF.
        std::snprintf(buf, sizeof buf,
                      "warning: OUT %02X <- %02X at PC=%04X: no board decodes port 0x%02X. "
                      "reads float to 0xFF.",
                      port, c.data, instrPc_, port);
    else
        // e.g. warning: IN 0FE -> FF at PC=0113: no board decodes port 0xFE. it floated to 0xFF.
        std::snprintf(buf, sizeof buf,
                      "warning: IN %02X -> %02X at PC=%04X: no board decodes port 0x%02X. "
                      "it floated to 0xFF.",
                      port, c.data, instrPc_, port);
    log_.push_back(buf);

    // Halt names the same access to the run loop, which stops at the boundary.
    if (unclaimedPolicy_ == Unclaimed::Halt) {
        unclaimedHalt_ = true;
        haltPort_ = port;
        haltWrite_ = write;
    }
}

// THE CYCLE HANDED TO settle() IS THE FINISHED ONE, AND `data` IS ALWAYS VALID HERE.
//
// BusCycle::data is documented as "valid on writes", and during the cycle that is
// true -- on a READ nobody has driven the bus yet when decodes() and read() are
// asked. But settle() runs AFTER the cycle completes (board.h, snoop()), and by then
// a byte HAS been driven: by the board that answered, or by the floating bus, which
// drives 0xFF just as surely. So every read path back-fills `data` with what came
// back before calling this.
//
// That is not a convenience for the snoopers. It is what the wire is doing. The
// front panel's DATA lamps are eight LEDs soldered to D0..D7 and they do not care
// which direction the byte was going -- and neither does a TRACE, which is why the
// observers get the same corrected cycle.
// THE FALLBACK status word, derivable from the cycle TYPE alone -- used only when no
// master annotated the cycle (a monitor memory poke or a DMA transfer, neither of
// which asserts an 8080 status word). The 8080/Z80 CPU is the real GENERATOR: it asserts the
// full word (M1, STACK, ...) on the cycle it originates, and this fallback is a subset
// of that word, so settle()'s OR (below) never corrupts a master-supplied one. WO* is
// active low: set on reads/inputs, clear on writes/outputs. M1/HLTA/STACK are never in
// the fallback -- they need CPU knowledge the cycle type discards (see Status8080).
static uint8_t statusFor(Cycle t) {
    switch (t) {
        case Cycle::MemRead:  return StMemR | StWo;  // 0x82
        case Cycle::MemWrite: return 0;              // WO* = 0 on a write
        case Cycle::IoRead:   return StInp  | StWo;  // 0x42
        case Cycle::IoWrite:  return StOut;          // 0x10, WO* = 0
        case Cycle::IntAck:   return StInta | StWo;  // 0x03 (an input-like cycle)
    }
    return 0;
}

void Bus::settle(BusCycle& c) {
    // Latch the 8080 status word onto the cycle, right where `data` was back-filled,
    // so snoopers and observers see a complete cycle. OR, not assign: the CPU already
    // set the full word at the origin (M1/STACK included) and statusFor() is only the
    // TYPE-derived FALLBACK -- a subset of any master word, so the OR never clobbers
    // it, and it fills in the whole word for an un-annotated cycle (see Status8080).
    c.status |= statusFor(c.type);

    // ONLY the cards that actually watch. Every board still SEES every cycle --
    // that is what a backplane is -- but a card with nothing wired to the address
    // bus latches nothing, and calling its do-nothing snoop() to discover that,
    // once per card per cycle, was pure ceremony. wantsSnoop() is the card telling
    // us it has a flip-flop on that bus. Almost none do.
    for (Board* b : snoopers_)
        if (b->enabled()) b->snoop(c);

    // And anyone watching from OUTSIDE the backplane -- the debugger, the tracer.
    // They see exactly what the cards see, because it is the same stream. That is
    // why BREAK IO and BREAK MEM needed no new machinery and cost no CPU support:
    // a bus cycle is a bus cycle no matter who originated it, so they work against
    // a DMA transfer and a front-panel DEPOSIT, not just against the processor.
    for (const auto& o : observers_) o.second(c);
}

int Bus::observe(Observer fn) {
    int h = nextObserver_++;
    observers_.emplace_back(h, std::move(fn));
    return h;
}

void Bus::unobserve(int handle) {
    observers_.erase(std::remove_if(observers_.begin(), observers_.end(),
                                    [&](const auto& o) { return o.first == handle; }),
                     observers_.end());
}

// READ PIN 73. Is anybody pulling it down?
//
// Not a question we ask the boards -- they TOLD us, when it changed. That is what
// a wire is, and it is the whole difference between this and a survey.
bool Bus::intPending() const {
    if (verify_) verifyInt();
    return intCount_ > 0;
}

bool Bus::nmiAsserted() const {
    if (verify_) verifyNmi();
    return nmiCount_ > 0;
}

bool Bus::holdPending() const {
    if (verify_) verifyHold();
    return holdCount_ > 0;
}

// The proof for NMI*, the counterpart of verifyInt(). A board that moved its
// NMI level and forgot nmiChanged() loses the edge -- the NMI simply never happens.
void Bus::verifyNmi() const {
    int live = 0;
    for (Board* b : boards_) {
        bool actual = b->enabled() && b->assertsNmi();
        if (b->nmiWire()) ++live;
        if (actual == b->nmiWire()) continue;

        std::fprintf(stderr,
                     "\nNMI* WIRE IS STALE -- board '%s'\n"
                     "  the wire says: %s\n"
                     "  the board says: %s\n"
                     "It changed its NMI state and did not call nmiChanged().\n",
                     b->id.c_str(), b->nmiWire() ? "pulling" : "not pulling",
                     actual ? "pulling" : "not pulling");
        std::abort();
    }

    if (live != nmiCount_) {
        std::fprintf(stderr,
                     "\nNMI* WIRE-OR COUNT IS WRONG\n"
                     "  the bus thinks %d board(s) are pulling NMI*\n"
                     "  %d actually are\n"
                     "A board went into or out of the backplane without the count following.\n",
                     nmiCount_, live);
        std::abort();
    }
}

// The proof for pHOLD, the exact counterpart of verifyInt(): re-derive the wire from
// every board's requestsBus() and abort on the first board -- or the count -- that
// disagrees with what we cached. A board that starts or stops wanting the bus and
// forgets to call holdChanged() would not hang the guest the way a stale pin 73 does;
// it would do something quieter -- the transfer never runs, because the fast wire said
// nobody asked -- and that is the kind of silence this mode exists to break.
void Bus::verifyHold() const {
    int live = 0;
    for (Board* b : boards_) {
        bool actual = b->enabled() && b->requestsBus();
        if (b->holdWire()) ++live;
        if (actual == b->holdWire()) continue;

        std::fprintf(stderr,
                     "\npHOLD WIRE IS STALE -- board '%s'\n"
                     "  the wire says: %s\n"
                     "  the board says: %s\n"
                     "It changed its bus-request state and did not call holdChanged().\n",
                     b->id.c_str(), b->holdWire() ? "pulling" : "not pulling",
                     actual ? "pulling" : "not pulling");
        std::abort();
    }

    if (live != holdCount_) {
        std::fprintf(stderr,
                     "\npHOLD WIRE-OR COUNT IS WRONG\n"
                     "  the bus thinks %d board(s) are pulling pin 74\n"
                     "  %d actually are\n"
                     "A board went into or out of the backplane without the count following.\n",
                     holdCount_, live);
        std::abort();
    }
}

// READ VI0-VI7. Which of the eight are being pulled down?
//
// Same wire, same rules, same cache. The bus reports the lines and stops there; an
// 88-VI decides what they MEAN.
// It does NOT verify here, and that is deliberate: an 88-VI reads this from inside
// its own assertsInt(), which verifyInt() is in the middle of calling. Verifying the
// VI wires from verifyInt() instead (where it happens once per instruction) checks
// exactly the same thing without a card being asked to prove itself mid-answer.
uint8_t Bus::viLines() const { return viMask_; }

// A VI line moved. Tell the cards that watch them -- an 88-VI's pin 73 is a function
// of these eight wires, and it was not the one who moved them.
void Bus::notifyViWatchers() {
    if (inViNotify_) return;
    inViNotify_ = true;
    for (Board* b : boards_)
        if (b->watchesVi()) b->intChanged();
    inViNotify_ = false;
}

// The proof, and the exact counterpart of verifySlot(). Re-derive the wire from
// every board's combinational assertsInt() and compare it to what each board says
// it is driving, and to the running count.
//
// A board that changed its interrupt state and forgot to call intChanged() dies
// HERE, loudly, at the next instruction -- rather than hanging the guest in a HLT
// forever, waiting for an interrupt that already happened and was never carried.
// That is a bug worth days, and it would present as "the emulator locks up
// sometimes", which is worth several more.
void Bus::verifyInt() const {
    int live = 0;
    for (Board* b : boards_) {
        bool actual = b->enabled() && b->assertsInt();
        if (b->intWire()) ++live;
        if (actual == b->intWire()) continue;

        std::fprintf(stderr,
                     "\npINT WIRE IS STALE -- board '%s'\n"
                     "  the wire says: %s\n"
                     "  the board says: %s\n"
                     "It changed its interrupt state and did not call intChanged().\n",
                     b->id.c_str(), b->intWire() ? "pulling" : "not pulling",
                     actual ? "pulling" : "not pulling");
        std::abort();
    }

    // ...and the count itself, which attach()/detach() maintain by hand. Two
    // boards whose errors cancel would sail past the loop above; a bad count would
    // not, and this is the only place either could be caught.
    if (live != intCount_) {
        std::fprintf(stderr,
                     "\npINT WIRE-OR COUNT IS WRONG\n"
                     "  the bus thinks %d board(s) are pulling pin 73\n"
                     "  %d actually are\n"
                     "A board went into or out of the backplane without the count following.\n",
                     intCount_, live);
        std::abort();
    }

    // Pin 73 is not the only interrupt wire any more, and a VI line that goes stale
    // hangs the guest in exactly the same way. Checked from here so it is checked
    // every instruction (intPending() is), not only when an 88-VI happens to look.
    verifyVi();
}

// The same proof, eight wires wide.
void Bus::verifyVi() const {
    int live[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for (Board* b : boards_) {
        uint8_t actual = b->enabled() ? b->assertsVi() : 0;
        for (int i = 0; i < 8; ++i)
            if (b->viWire() & (1u << i)) ++live[i];
        if (actual == b->viWire()) continue;

        std::fprintf(stderr,
                     "\nVI WIRES ARE STALE -- board '%s'\n"
                     "  the wires say: %02X\n"
                     "  the board says: %02X\n"
                     "It changed its interrupt state and did not call intChanged().\n",
                     b->id.c_str(), b->viWire(), actual);
        std::abort();
    }

    for (int i = 0; i < 8; ++i) {
        if (live[i] == viCount_[i]) continue;
        std::fprintf(stderr,
                     "\nVI%d WIRE-OR COUNT IS WRONG\n"
                     "  the bus thinks %d board(s) are pulling it\n"
                     "  %d actually are\n"
                     "A board went into or out of the backplane without the count following.\n",
                     i, viCount_[i], live[i]);
        std::abort();
    }
}

// ---------------------------------------------------------------------------
// The cycle. This is the whole bus.
//
//   pass 1  resolve PHANTOM* -- ask every board whether it pulls the pin
//   pass 2  decode          -- ask every board whether it drives; move the byte
//   pass 3  settle          -- show the finished cycle to every board, so any
//                             card watching the address bus can latch what it saw
//
// Pass 3 exists because a real card can watch a cycle it does not answer, and
// the Tarbell does exactly that. It is not a callback and it is not the bus
// telling anyone anything: the cycle was on the backplane, in front of every
// card, the entire time.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// THE EXACT PATH. This is the definition of correctness, and it is the code that
// was here before -- three passes, every board asked, nothing cached. The tables
// below are a CACHE OF THIS, and the verifier checks them AGAINST it.
//
// A read is answered by the FIRST board in slot order that drives it. A write is
// latched by EVERY board that drives it -- two cards both latching the same write
// is a real fault and we reproduce it rather than pick a winner.
//
// The n > 1 arm re-asks decoders(). That calls decodes() a second time, which is
// free of consequence because decodes() is COMBINATIONAL AND PURE by contract
// (board.h) -- and it happens only when the machine is already electrically
// broken and we are on our way to printing a paragraph about it.
// ---------------------------------------------------------------------------

uint8_t Bus::memReadExact(uint16_t addr, uint8_t ext, uint8_t status) {
    BusCycle c{Cycle::MemRead, addr, 0, false};
    c.ext = ext;
    c.status = status;  // the master's asserted word; settle() ORs the fallback base on
    c.phantom = anyAssertsPhantom(c);

    Decode d = scan(c);
    if (d.n > 1) reportContention(c, decoders(c));

    unclaimed_ = (d.n == 0);
    contended_ = (d.n > 1);
    responder_ = d.first;
    uint8_t v = d.first ? d.first->read(c)
                        : 0xFF;  // floating bus (DESIGN.md 4.6.1)
    c.data = v;                  // what got driven -- see settle()
    settle(c);
    return v;
}

void Bus::memWriteExact(uint16_t addr, uint8_t ext, uint8_t data, uint8_t status) {
    BusCycle c{Cycle::MemWrite, addr, data, false};
    c.ext = ext;
    c.status = status;  // the master's asserted word; settle() ORs the fallback base on
    c.phantom = anyAssertsPhantom(c);

    Decode d = scan(c);
    if (d.n > 1) reportContention(c, decoders(c));

    unclaimed_ = (d.n == 0);
    contended_ = (d.n > 1);
    responder_ = d.first;
    // Nobody latched it. The byte is simply gone -- the write half of the
    // floating bus. This is what a guest write to ROM does, and the bus needed
    // no rule about ROM to make it happen.
    if (d.n == 1) d.first->write(c);
    else if (d.n > 1) for (Board* b : decoders(c)) b->write(c);
    settle(c);
}

uint8_t Bus::ioReadExact(uint16_t addr, uint8_t status) {
    BusCycle c{Cycle::IoRead, addr, 0, false};
    c.status = status;  // the master's asserted word; settle() ORs the fallback base on
    Decode d = scan(c);
    unclaimed_ = (d.n == 0);
    contended_ = false;
    responder_ = d.first;
    uint8_t v  = 0xFF;
    if (d.n > 0) {
        // Each driver puts its own lines on the bus; a line nobody drives floats to 1
        // (DESIGN.md 4.6.1). Two drivers on one line is contention (4.6): reported, and the
        // first driver's byte is what the guest gets, as before.
        std::vector<Board*> who = d.n > 1 ? decoders(c) : std::vector<Board*>{d.first};
        std::vector<uint8_t> mask;
        uint8_t              all = 0;
        for (Board* b : who) {
            const uint8_t m = b->drivesBits(c);
            if (m & all) contended_ = true;
            all |= m;
            mask.push_back(m);
        }
        if (contended_) {
            reportContention(c, who);
            v = d.first->read(c);
        } else {
            v = (uint8_t)~all;
            for (size_t i = 0; i < who.size(); ++i) v |= (uint8_t)(who[i]->read(c) & mask[i]);
        }
    }
    c.data = v;  // what got driven -- see settle()
    if (unclaimedPolicy_ != Unclaimed::Silent && d.n == 0 && !localIo_) reportUnclaimed(c);
    settle(c);
    return v;
}

void Bus::ioWriteExact(uint16_t addr, uint8_t data, uint8_t status) {
    BusCycle c{Cycle::IoWrite, addr, data, false};
    c.status = status;  // the master's asserted word; settle() ORs the fallback base on
    Decode d = scan(c);
    std::vector<Board*> who;
    if (d.n > 1) who = decoders(c);
    const bool shared = d.n > 1 && sharedWrite(c, who);
    if (d.n > 1 && !shared) reportContention(c, who);
    unclaimed_ = (d.n == 0);
    contended_ = (d.n > 1 && !shared);
    responder_ = d.first;
    if (d.n == 1) d.first->write(c);
    else if (d.n > 1) for (Board* b : who) b->write(c);
    if (unclaimedPolicy_ != Unclaimed::Silent && d.n == 0 && !localIo_) reportUnclaimed(c);
    settle(c);
}

// ---------------------------------------------------------------------------
// THE CACHE. Built by asking the boards -- the same two questions, in slot order.
// ---------------------------------------------------------------------------

Bus::Slot Bus::resolve(Cycle t, uint16_t addr, uint8_t ext) const {
    BusCycle c{t, addr, 0, false};
    c.ext = ext;
    c.phantom = anyAssertsPhantom(c);  // pass 1, exactly as a live cycle does it
    Decode d = scan(c);                // pass 2, likewise

    Slot s;
    s.phantom = c.phantom;
    s.who     = d.first;
    s.slow    = exactRead(c, d);  // contention or a partial driver: the exact path handles it
    return s;
}

bool Bus::exactRead(const BusCycle& c, const Decode& d) const {
    if (d.n > 1) return true;
    if (!d.first) return false;
    // A port shared by design is never cached: the sharing board's drivesBits() follows its
    // own state (board.h, sharesPort()), so only the exact path asks it at the right time.
    const bool io = c.type == Cycle::IoRead || c.type == Cycle::IoWrite;
    if (io && d.first->sharesPort(c)) return true;
    return c.type == Cycle::IoRead && d.first->drivesBits(c) != 0xFF;
}

// An I/O write that several boards decode is contention unless EVERY one of them shares
// the port by design (board.h, sharesPort()). The bus master drives the data lines on a
// write; the question is only whether the boards meant to listen together.
bool Bus::sharedWrite(const BusCycle& c, const std::vector<Board*>& who) const {
    if (c.type != Cycle::IoWrite) return false;
    for (Board* b : who)
        if (!b->sharesPort(c)) return false;
    return true;
}

void Bus::buildPages(uint8_t ext, Slot* rd, Slot* wr) const {
    // IS ANY CARD IN THIS MACHINE DECODING A LOW ADDRESS LINE?
    //
    // Almost never. But the Tarbell gates its PROM and its PHANTOM* with A5, so it
    // answers 0x0000-0x001F and not 0x0020-0x003F -- two answers inside page 0. We
    // do not ASSUME our way past that and we do not pick a finer page size and hope
    // (the next such card will decode A0). We ask, and if the answer is yes we stop
    // guessing and PROBE.
    bool uniform = true;
    for (Board* b : boards_)
        if (!b->decodeIsPageUniform()) uniform = false;

    for (int p = 0; p < 256; ++p) {
        uint16_t a = (uint16_t)(p << 8);
        rd[p]      = resolve(Cycle::MemRead, a, ext);
        wr[p]      = resolve(Cycle::MemWrite, a, ext);

        if (uniform) continue;

        // Probe all 256 addresses of the page. If they do not agree, the page has
        // no single answer, so it gets NO cached answer: it is served by the exact
        // two-pass path forever, which is the code that was always here.
        //
        // This costs a few milliseconds, it happens only on a rebuild (an operator
        // action, not a guest one), and only in a machine that actually contains
        // such a card. The Tarbell costs us ONE page out of 256.
        for (int lo = 1; lo < 256; ++lo) {
            uint16_t x = (uint16_t)(a | lo);
            Slot r = resolve(Cycle::MemRead, x, ext);
            Slot w = resolve(Cycle::MemWrite, x, ext);
            if (r.who != rd[p].who || r.phantom != rd[p].phantom || r.slow != rd[p].slow)
                rd[p].slow = true;
            if (w.who != wr[p].who || w.phantom != wr[p].phantom || w.slow != wr[p].slow)
                wr[p].slow = true;
        }
    }
}

// IEEE 696: a segment's own tables, built the first time a cycle reaches it -- only in a
// machine where some board decodes A16-A23 (pageSlot). rebuild() drops them all, so a
// decode change reaches every segment the same way it reaches segment 0.
Bus::SegTables& Bus::segTables(uint8_t ext) {
    std::unique_ptr<SegTables>& t = seg_[ext];
    if (!t) {
        t = std::make_unique<SegTables>();
        buildPages(ext, t->rd, t->wr);
    }
    return *t;
}

void Bus::rebuild() {
    dirty_ = false;
    buildPages(0, memRead_, memWrite_);
    for (auto& t : seg_) t.reset();  // the other segments rebuild on their next cycle

    // Does any board decode A16-A23? Nearly never: then every segment has segment 0's
    // answer and shares its tables (pageSlot), so no segment table is ever built.
    extDecode_ = false;
    for (Board* b : boards_)
        if (b->decodesExtended()) extDecode_ = true;

    // I/O: all 256 ports, each probed exactly. No contract, no assumption -- a
    // card may decode any port it pleases, including one.
    for (int p = 0; p < 256; ++p) {
        ioRead_[p]  = resolve(Cycle::IoRead, (uint16_t)p);
        ioWrite_[p] = resolve(Cycle::IoWrite, (uint16_t)p);
        // A board that decodes A8-A15 on this low byte (Board::decodesIoHigh): a slot built
        // with A8-A15 = 0 cannot hold its answer, so every cycle here takes the exact path,
        // which passes the whole address.
        for (Board* b : boards_)
            if (b->decodesIoHigh((uint8_t)p)) {
                ioRead_[p].slow = ioWrite_[p].slow = true;
                break;
            }
    }

    snoopers_.clear();
    for (Board* b : boards_)
        if (b->wantsSnoop()) snoopers_.push_back(b);
}

// The proof. Re-derive the decode the slow way and compare it to what we cached.
// A board that changed its decode without saying so dies HERE, loudly, at the
// first cycle that touches it -- instead of somewhere downstream, quietly, in a
// month. Costs more than the original code; it is not a path, it is an assertion.
void Bus::verifySlot(const BusCycle& in, const Slot& s) const {
    BusCycle c = in;
    c.phantom = anyAssertsPhantom(c);
    Decode d = scan(c);

    bool ok = (d.first == s.who) && (c.phantom == s.phantom) && (exactRead(c, d) == s.slow);
    if (ok) return;

    std::fprintf(stderr,
                 "\nBUS DECODE CACHE IS STALE at %s 0x%0*X\n"
                 "  cached: who=%s phantom=%d slow=%d\n"
                 "  actual: who=%s phantom=%d n=%d\n"
                 "A board changed its decode and did not call decodeChanged().\n",
                 cycleName(c.type), c.ext ? 6 : 4, (unsigned)c.phys(), s.who ? s.who->id.c_str() : "-", (int)s.phantom,
                 (int)s.slow, d.first ? d.first->id.c_str() : "-", (int)c.phantom, d.n);
    std::abort();
}

// ---------------------------------------------------------------------------
// THE CYCLE. A table lookup, and the board that the table names.
// ---------------------------------------------------------------------------

// The shared body of a cached memory cycle. `s` is the slot for this page in this
// segment, and it is not slow (the caller sent a slow page down the exact path).
inline uint8_t Bus::readSlot(const Slot& s, uint16_t addr, uint8_t ext, uint8_t status) {
    BusCycle c{Cycle::MemRead, addr, 0, s.phantom};
    c.ext = ext;
    c.status = status;  // the master's asserted word; settle() ORs the fallback base on
    unclaimed_ = (s.who == nullptr);
    responder_ = s.who;
    uint8_t v = s.who ? s.who->read(c) : 0xFF;  // floating bus (DESIGN.md 4.6.1)
    c.data = v;                                 // what got driven -- see settle()
    settle(c);
    return v;
}

inline void Bus::writeSlot(const Slot& s, uint16_t addr, uint8_t ext, uint8_t data,
                           uint8_t status) {
    BusCycle c{Cycle::MemWrite, addr, data, s.phantom};
    c.ext = ext;
    c.status = status;  // the master's asserted word; settle() ORs the fallback base on
    unclaimed_ = (s.who == nullptr);
    responder_ = s.who;
    // Nobody latched it. The byte is simply gone -- the write half of the floating
    // bus, and what a guest write to ROM does. No rule about ROM was needed.
    if (s.who) s.who->write(c);
    settle(c);
}

// The slot for a memory page in a segment. Segment 0 -- and EVERY segment when no board
// decodes A16-A23 -- is memRead_/memWrite_; only a machine with such a board builds a
// segment's own tables. A board that decodes A16-A23 and does not say so
// (Board::decodesExtended) is caught by setVerify(true): verifySlot re-derives the decode
// with the real A16-A23 and aborts when the shared answer is wrong.
inline const Bus::Slot& Bus::pageSlot(Cycle t, uint8_t ext, uint16_t addr) {
    if (ext == 0 || !extDecode_)
        return t == Cycle::MemRead ? memRead_[addr >> 8] : memWrite_[addr >> 8];
    SegTables& st = segTables(ext);
    return t == Cycle::MemRead ? st.rd[addr >> 8] : st.wr[addr >> 8];
}

// THE MEMORY CYCLE above segment 0 (memReadX/memWriteX). Segment 0 has its own body
// below, the hot path; this one is for the IEEE 696 segments.
inline uint8_t Bus::readAt(uint16_t addr, uint8_t ext, uint8_t status) {
    // A cycle breakpoint that stops BEFORE the access unwinds the instruction here,
    // before any board is touched (see setPreAccessVeto / CycleBreakBefore). One
    // null-function test when no such breakpoint is armed.
    BusCycle probe{Cycle::MemRead, addr, 0, false};
    probe.ext = ext;
    if (preVeto_ && preVeto_(probe)) throw CycleBreakBefore{};
    if (dirty_) rebuild();
    const Slot& s = pageSlot(Cycle::MemRead, ext, addr);

    // A slow page has no cached answer to check -- that is what makes it slow.
    if (s.slow) return memReadExact(addr, ext, status);  // contention, or a non-uniform page.
    if (verify_) verifySlot(probe, s);
    return readSlot(s, addr, ext, status);
}

inline void Bus::writeAt(uint16_t addr, uint8_t ext, uint8_t data, uint8_t status) {
    BusCycle probe{Cycle::MemWrite, addr, data, false};
    probe.ext = ext;
    if (preVeto_ && preVeto_(probe)) throw CycleBreakBefore{};
    if (dirty_) rebuild();
    const Slot& s = pageSlot(Cycle::MemWrite, ext, addr);

    if (s.slow) { memWriteExact(addr, ext, data, status); return; }
    if (verify_) verifySlot(probe, s);
    writeSlot(s, addr, ext, data, status);
}

// SEGMENT 0 -- the path every machine and every CPU core runs -- keeps its own
// body, the code that was here before IEEE 696: it builds the probe cycle only when a
// veto is armed and indexes memRead_/memWrite_ directly. Routing it through readAt()
// measured about 3-5% slower on ZEXALL (interleaved A/B, 2026-11-02), so it does not.
uint8_t Bus::memRead0(uint16_t addr, uint8_t status) {
    // A cycle breakpoint that stops BEFORE the access unwinds the instruction here,
    // before any board is touched (see setPreAccessVeto / CycleBreakBefore). One
    // null-function test when no such breakpoint is armed.
    if (preVeto_ && preVeto_(BusCycle{Cycle::MemRead, addr, 0, false})) throw CycleBreakBefore{};
    if (dirty_) rebuild();
    const Slot& s = memRead_[addr >> 8];

    // A slow page has no cached answer to check -- that is what makes it slow.
    if (s.slow) return memReadExact(addr, 0, status);  // contention, or a non-uniform page.
    if (verify_) verifySlot(BusCycle{Cycle::MemRead, addr, 0, false}, s);
    return readSlot(s, addr, 0, status);
}

void Bus::memWrite0(uint16_t addr, uint8_t data, uint8_t status) {
    if (preVeto_ && preVeto_(BusCycle{Cycle::MemWrite, addr, data, false})) throw CycleBreakBefore{};
    if (dirty_) rebuild();
    const Slot& s = memWrite_[addr >> 8];

    if (s.slow) { memWriteExact(addr, 0, data, status); return; }
    if (verify_) verifySlot(BusCycle{Cycle::MemWrite, addr, data, false}, s);
    writeSlot(s, addr, 0, data, status);
}

// IEEE 696: the physical cycles with A16-A23. Never translated.
uint8_t Bus::memReadX(uint32_t phys, uint8_t status) {
    phys &= addrMask();
    return readAt((uint16_t)phys, (uint8_t)(phys >> 16), status);
}

void Bus::memWriteX(uint32_t phys, uint8_t data, uint8_t status) {
    phys &= addrMask();
    writeAt((uint16_t)phys, (uint8_t)(phys >> 16), data, status);
}

// Look without a cycle, above 64K. Not a hot path (the monitor, not the instruction
// recorder), so it takes the exact first-responder scan, the same as peek() does when
// it has no cache to trust -- and stays const, building no segment table.
uint8_t Bus::peekX(uint32_t phys) const {
    phys &= addrMask();
    const uint16_t addr = (uint16_t)phys;
    const uint8_t ext   = (uint8_t)(phys >> 16);
    if (ext == 0) return peek(addr);

    BusCycle c{Cycle::MemRead, addr, 0, false};
    c.ext = ext;
    c.phantom = anyAssertsPhantom(c);
    for (Board* b : boards_)
        if (b->enabled() && b->decodes(c)) {
            uint8_t v = 0xFF;
            if (b->peekX(phys, v)) return v;  // Board::peekX: peek() unless it decodes A16-A23
        }
    return 0xFF;
}

// ---------------------------------------------------------------------------
// IEEE 696: the 16-bit transfer (2.6). One cycle when the decoding slave answers
// SIXTN*; otherwise nothing at all, and the master does the byte-serial response.
// ---------------------------------------------------------------------------

Board* Bus::decoderFor16(BusCycle& c) {
    if (dirty_) rebuild();
    const Slot& s = pageSlot(c.type, c.ext, c.addr);
    if (!s.slow) {
        if (verify_) verifySlot(c, s);
        c.phantom = s.phantom;
        return s.who;
    }
    // A slow page: the exact scan. Contention gets no 16-bit cycle -- the byte-serial
    // cycles that follow go down the exact path, which reports it.
    c.phantom = anyAssertsPhantom(c);
    Decode d = scan(c);
    return d.n == 1 ? d.first : nullptr;
}

Transfer16 Bus::memRead16(uint32_t phys, uint8_t status) {
    Transfer16 r;
    phys &= addrMask();
    // A word is at an even address (A0 = 0 for word transfers, 2.6.4.2). An odd one: no
    // 16-bit cycle, nothing driven.
    if (phys & 1) return r;

    BusCycle c{Cycle::MemRead, (uint16_t)phys, 0, false};
    c.ext    = (uint8_t)(phys >> 16);
    c.xtrq   = true;
    c.status = status;
    // Decode first: decodes() and answersSixtn() are pure, so asking strobes nobody. With
    // no SIXTN* no cycle runs at all -- not even through the breakpoint veto, which then
    // sees the byte-serial cycles that do run, once each. lastUnclaimed()/lastResponder()
    // keep the previous cycle's values: there was no cycle to describe.
    Board* who = decoderFor16(c);
    if (!who || !who->answersSixtn(c)) return r;  // no SIXTN*: the master goes byte-serial
    if (preVeto_ && preVeto_(c)) throw CycleBreakBefore{};

    unclaimed_ = false;
    contended_ = false;
    responder_ = who;
    who->read16(c, r.even, r.odd);
    r.sixtn  = true;
    c.data   = r.even;  // what got driven -- see settle()
    c.dataHi = r.odd;
    settle(c);
    return r;
}

bool Bus::memWrite16(uint32_t phys, uint8_t even, uint8_t odd, uint8_t status) {
    phys &= addrMask();
    if (phys & 1) return false;

    BusCycle c{Cycle::MemWrite, (uint16_t)phys, even, false};
    c.ext    = (uint8_t)(phys >> 16);
    c.xtrq   = true;
    c.dataHi = odd;
    c.status = status;
    Board* who = decoderFor16(c);  // decode first, as in memRead16
    if (!who || !who->answersSixtn(c)) return false;
    if (preVeto_ && preVeto_(c)) throw CycleBreakBefore{};

    unclaimed_ = false;
    contended_ = false;
    responder_ = who;
    who->write16(c);
    settle(c);
    return true;
}

Transfer16 Bus::byteSerialRead16(uint32_t phys, bool evenFirst, uint8_t status) {
    Transfer16 r;
    const uint32_t even = phys & ~1u, odd = even + 1;
    if (evenFirst) {
        r.even = memReadX(even, status);
        r.odd  = memReadX(odd, status);
    } else {
        r.odd  = memReadX(odd, status);
        r.even = memReadX(even, status);
    }
    return r;
}

void Bus::byteSerialWrite16(uint32_t phys, uint8_t even, uint8_t odd, bool evenFirst,
                            uint8_t status) {
    const uint32_t e = phys & ~1u, o = e + 1;
    if (evenFirst) {
        memWriteX(e, even, status);
        memWriteX(o, odd, status);
    } else {
        memWriteX(o, odd, status);
        memWriteX(e, even, status);
    }
}

// Out of line on purpose: see memRead in bus.h.
uint8_t Bus::memReadMapped(uint16_t addr, uint8_t status) {
    return xlat_->readData(memReadX(xlat_->translate(addr, Cycle::MemRead), status));
}

void Bus::memWriteMapped(uint16_t addr, uint8_t data, uint8_t status) {
    memWriteX(xlat_->translate(addr, Cycle::MemWrite), data, status);
}

uint8_t Bus::ioRead(uint16_t addr, uint8_t status) {
    // A mapped board's own core: the board decides what goes on A0-A15 (AddressTranslator).
    if (xlat_) [[unlikely]] addr = xlat_->ioAddress(addr, Cycle::IoRead);
    // IEEE 696 keeps all of A0-A15 on an I/O cycle (extended I/O, 2.2.2.4).
    if (preVeto_ && preVeto_(BusCycle{Cycle::IoRead, addr, 0, false})) throw CycleBreakBefore{};
    localIo_ = xlat_ && xlat_->ownsIo(addr, Cycle::IoRead);
    if (localIo_) [[unlikely]] {
        (void)ioReadBus(addr, status);   // the S-100 cycle still runs; its data floats past
        unclaimed_ = false;
        localIo_ = false;
        return xlat_->localRead(addr);
    }
    return ioReadBus(addr, status);
}

inline uint8_t Bus::ioReadBus(uint16_t addr, uint8_t status) {
    if (dirty_) rebuild();
    const Slot& s = ioRead_[addr & 0xFF];

    if (s.slow) return ioReadExact(addr, status);
    if (verify_) verifySlot(BusCycle{Cycle::IoRead, addr, 0, false}, s);

    BusCycle c{Cycle::IoRead, addr, 0, false};
    c.status = status;  // the master's asserted word; settle() ORs the fallback base on
    unclaimed_ = (s.who == nullptr);
    responder_ = s.who;
    uint8_t v = s.who ? s.who->read(c) : 0xFF;
    c.data = v;  // what got driven -- see settle()
    if (unclaimedPolicy_ != Unclaimed::Silent && s.who == nullptr && !localIo_) reportUnclaimed(c);
    settle(c);
    return v;
}

void Bus::ioWrite(uint16_t addr, uint8_t data, uint8_t status) {
    // A mapped board's own core: the board decides what goes on A0-A15 (AddressTranslator).
    if (xlat_) [[unlikely]] addr = xlat_->ioAddress(addr, Cycle::IoWrite);
    if (preVeto_ && preVeto_(BusCycle{Cycle::IoWrite, addr, data, false})) throw CycleBreakBefore{};
    localIo_ = xlat_ && xlat_->ownsIo(addr, Cycle::IoWrite);
    if (localIo_) [[unlikely]] {
        ioWriteBus(addr, data, status);  // the S-100 cycle still runs
        unclaimed_ = false;
        localIo_ = false;
        xlat_->localWrite(addr, data);
        return;
    }
    ioWriteBus(addr, data, status);
}

inline void Bus::ioWriteBus(uint16_t addr, uint8_t data, uint8_t status) {
    if (dirty_) rebuild();
    const Slot& s = ioWrite_[addr & 0xFF];

    if (s.slow) { ioWriteExact(addr, data, status); return; }
    if (verify_) verifySlot(BusCycle{Cycle::IoWrite, addr, data, false}, s);

    BusCycle c{Cycle::IoWrite, addr, data, false};
    c.status = status;  // the master's asserted word; settle() ORs the fallback base on
    unclaimed_ = (s.who == nullptr);
    responder_ = s.who;
    if (s.who) s.who->write(c);
    if (unclaimedPolicy_ != Unclaimed::Silent && s.who == nullptr && !localIo_) reportUnclaimed(c);
    settle(c);
}

uint8_t Bus::intAck(uint8_t status) {
    BusCycle c{Cycle::IntAck, 0, 0, false};
    c.status = status;  // the master's asserted word; settle() ORs the fallback base on
    auto who = decoders(c);
    unclaimed_ = who.empty();
    contended_ = who.size() > 1;
    responder_ = who.empty() ? nullptr : who.front();
    // No vector-interrupt board answered, so nothing drives the bus and it
    // floats high: 0xFF, which the 8080 executes as RST 7. That is the real
    // Altair's behavior, and we get it for free from the same rule that makes
    // unmapped memory read 0xFF. The bus does not know what a vector IS.
    uint8_t v = who.empty() ? 0xFF : who.front()->read(c);
    c.data = v;  // what got driven -- see settle()
    settle(c);
    return v;
}

} // namespace altair
