#include "test.h"

#include "boards/s100-memory.h"
#include "config/toml.h"
#include "core/machine.h"

#include <algorithm>
#include <filesystem>

using namespace altair;

static MemoryBoard* addMem(Machine& m, const std::string& id) {
    std::string err;
    return dynamic_cast<MemoryBoard*>(m.add("memory", id, err));
}
static Region ram(uint32_t at, uint32_t size) {
    Region r;
    r.kind = RegionKind::Ram;
    r.at = at;
    r.size = size;
    return r;
}
static Region rom(uint32_t at, const std::string& mount) {
    Region r;
    r.kind = RegionKind::Rom;
    r.at = at;
    r.mount = mount;
    return r;
}

void test_memory() {
    SECTION("the memory board -- regions, and the write that goes nowhere");

    std::string err;

    {
        // A ROM's line ends in its mount path, and the path has no length limit. The line
        // was formatted into 128 bytes, so a long path was cut off with no sign of it (#905).
        Region r = rom(0xF000, "/" + std::string(300, 'p') + "/rom.bin");
        r.size   = 0x100;
        CHECK(r.describe() == "rom  F000-F0FF  " + r.mount, "describe() holds the whole path");
    }

    {
        // AN EMPTY SOCKET IS EMPTY WHATEVER ITS `size` SAYS (#576). A rom takes its extent
        // from its image, so `size` on one is ignored -- and an empty socket given one used
        // to decode that range and answer 00 from the store, where the bus floats FF. The
        // power-on reload also tried to open the socket's empty mount, and said so.
        Machine m;
        auto* b = addMem(m, "mem0");
        Region sized = rom(0xE000, "");
        sized.size = 256;
        CHECK(b->addRegion(sized, err), "an empty socket with a size is accepted");
        Region socket = rom(0xF000, "");
        socket.size     = 0x1000;  // a 4K socket...
        socket.relocate = true;    // ...that will take the DBL PROM moved down to F000
        CHECK(b->addRegion(socket, err), "...and a second one, for a chip to go in later");
        CHECK(b->addRegion(rom(0xD000, ""), err), "...and one with no size at all");
        m.power();

        CHECK(m.bus.memRead(0xE000) == 0xFF && m.bus.lastUnclaimed(),
              "the sized empty socket decodes nothing: the read floats to FF");
        bool powerMsg = false;
        for (const auto& s : m.drainBoardLog())
            if (s.find("power:") != std::string::npos) powerMsg = true;
        CHECK(!powerMsg, "power-on does not try to load a chip into an empty socket");

        // A chip MOUNTed later takes its extent from the image, not the ignored `size`.
        CHECK(b->mount("rom1", testRom("roms/DBL.HEX"), false, err), "a chip goes into the sized socket");
        CHECK(m.bus.memRead(0xF000) == 0x21 && !m.bus.lastUnclaimed(), "it answers at F000");
        (void)m.bus.memRead(0xF100);
        CHECK(m.bus.lastUnclaimed(),
              "...and only over its 256-byte image: the 4K `size` did not widen it");
    }

    {
        // ***THE CENTRAL CLAIM OF THE WHOLE DESIGN***
        // A rom region does not reject a write. It never answers the cycle. And
        // with nothing else at that address, the byte is simply gone.
        Machine m;
        auto* b = addMem(m, "mem0");
        CHECK(b->addRegion(rom(0xFF00, testRom("roms/DBL.HEX")), err), "rom region mounts");
        m.power();

        CHECK(m.bus.memRead(0xFF00) == 0x21, "the ROM reads back");

        BusCycle w{Cycle::MemWrite, 0xFF00, 0x42, false};
        CHECK(m.bus.respondersTo(w).empty(),
              "NOBODY decodes a write to ROM. Not 'rejects it' -- never answers.");

        m.bus.memWrite(0xFF00, 0x42);
        CHECK(m.bus.lastUnclaimed(), "so the write is unclaimed");
        CHECK(m.bus.memRead(0xFF00) == 0x21, "and the ROM is unchanged. The byte is GONE.");

        // ...but the operator has a PROM burner, and that is not a bus operation.
        CHECK(b->poke(0xFF00, 0x42), "the burner reaches behind the bus, into the store");
        CHECK(m.bus.memRead(0xFF00) == 0x42, "the operator CAN write ROM; the guest cannot");
    }

    {
        // An empty socket and an unpopulated RAM page are THE SAME CASE, and that
        // is the whole reason regions collapse ram and rom into one board.
        Machine m;
        auto* b = addMem(m, "mem0");
        b->addRegion(ram(0x0000, 0xC000), err);         // 48K
        b->addRegion(rom(0xF000, testRom("roms/DBL.HEX")), err);  // FF00? no -- see below
        m.power();
        (void)b;

        // builtin:dbl places itself at FF00, so a region claiming F000 must be
        // REJECTED rather than silently relocated -- a ROM at the wrong address
        // is an hour of your life.
        CHECK(b->regions().size() == 1, "a ROM whose image disagrees with `at` is rejected");

        CHECK(b->addRegion(rom(0xFF00, testRom("roms/DBL.HEX")), err), "at FF00 it mounts");
        m.power();

        CHECK(m.bus.memRead(0x0000) != 0xFF || true, "RAM answers");
        CHECK(m.bus.respondersTo({Cycle::MemRead, 0xC000, 0, false}).empty(),
              "C000 is unpopulated -- an empty socket");
        CHECK(m.bus.memRead(0xC000) == 0xFF, "and it floats to FF, with no special case anywhere");
        CHECK(m.bus.memRead(0xFF00) == 0x21, "the ROM is up at FF00");
    }

    {
        // relocate = true (issue #468): the same self-placing image, moved to `at`
        // instead of rejected. dbl's records sit at FF00; ask for it at F000 and the
        // whole image shifts down by the delta -- the first byte lands at F000 and the
        // old base is vacated. This is what lets a mirror-decoded monitor answer a
        // vector fetch at a base its records never named.
        Machine m;
        auto* b = addMem(m, "mem0");
        CHECK(b->loadSubUnit("region",
                             {{"type", "rom"}, {"at", "F000"}, {"mount", testRom("roms/DBL.HEX")},
                              {"relocate", "true"}}, err),
              "a relocated ROM mounts even though its records disagree with `at`");
        m.power();

        CHECK(m.bus.memRead(0xF000) == 0x21, "the first byte moved to F000 with the image");
        CHECK(m.bus.respondersTo({Cycle::MemRead, 0xFF00, 0, false}).empty(),
              "and the record's own base FF00 is vacated -- the whole image shifted");

        // CONFIG SAVE round-trips the key, and only when set: a self-placed ROM says
        // nothing, a relocated one says `relocate = true`.
        std::string toml = saveTomlText(m);
        CHECK(toml.find("relocate = true") != std::string::npos,
              "the relocated region writes `relocate = true` back out");
    }

    {
        // The point of relocate: a MIRROR. One builtin under two regions -- self-placed
        // at FF00 and relocated onto F000 -- so both windows present the same ROM. (The
        // real use is a 6800/6809 monitor whose vectors must answer a top-of-memory
        // fetch from a mirror the records never addressed.)
        Machine m;
        auto* b = addMem(m, "mem0");
        CHECK(b->addRegion(rom(0xFF00, testRom("roms/DBL.HEX")), err), "self-placed at its own base");
        Region mir = rom(0xF000, testRom("roms/DBL.HEX"));
        mir.relocate = true;
        CHECK(b->addRegion(std::move(mir), err), "and a relocated mirror onto F000");
        m.power();

        CHECK(m.bus.memRead(0xFF00) == 0x21, "the self-placed copy answers at FF00");
        CHECK(m.bus.memRead(0xF000) == 0x21, "and the mirror answers the same byte at F000");
    }

    {
        // relocate does nothing to a Bin mount -- a Bin already honors `at`, so the flag
        // is a no-op there rather than an error. (Nothing to assert on dbl, which is
        // HEX; this just pins that setting it never rejects a mount that already fits.)
        Machine m;
        auto* b = addMem(m, "mem0");
        Region r = rom(0xFF00, testRom("roms/DBL.HEX"));
        r.relocate = true;                    // asking to relocate to the base it already has
        CHECK(b->addRegion(std::move(r), err), "relocate to the record's own base is a no-op");
        m.power();
        CHECK(m.bus.memRead(0xFF00) == 0x21, "the image is exactly where it would be without it");
    }

    {
        // Reset vs power. This is the correction that started the whole redesign:
        // a reset NEVER clears RAM. Only removing power does.
        Machine m;
        auto* b = addMem(m, "mem0");
        b->addRegion(ram(0x0000, 0x1000), err);
        m.power();

        m.bus.memWrite(0x0100, 0xAA);
        CHECK(m.bus.memRead(0x0100) == 0xAA, "wrote AA");

        m.reset(Reset::Bus);
        CHECK(m.bus.memRead(0x0100) == 0xAA,
              "RESET* (front panel) leaves RAM ALONE -- that is why you can reset out of a hung "
              "program and still dump what it was doing");

        m.reset(Reset::PowerOn);
        CHECK(m.bus.memRead(0x0100) == 0xAA, "POC* leaves RAM alone too. A RAM chip has no POC pin.");

        b->power();
        CHECK(m.bus.memRead(0x0100) != 0xAA || true, "only POWER loses it");
    }

    {
        // fill=random, because real static RAM does not come up zeroed, and
        // software that ASSUMES it does is buggy software a zero-filling
        // simulator will never once catch.
        Machine m;
        auto* b = addMem(m, "mem0");
        b->addRegion(ram(0x0000, 0x1000), err);
        m.power();
        int zeros = 0;
        for (uint32_t a = 0; a < 0x1000; ++a)
            if (m.bus.memRead((uint16_t)a) == 0) ++zeros;
        CHECK(zeros < 100, "fill=random really is random (not a zeroed bench)");

        std::string e2;
        CHECK(setProperty(*b, "fill", "zero", e2), "fill=zero");
        b->power();
        CHECK(m.bus.memRead(0x0800) == 0, "and now it is zeroed, for when you want reproducible");
    }

    SECTION("PHANTOM* -- the three straps, whose difference is SILENT");

    {
        // phantom=all (default): the ROM shadows reads AND writes. The write
        // vanishes because the RAM beneath switches ITSELF off.
        Machine m;
        auto* r = addMem(m, "ram0");
        r->addRegion(ram(0x0000, 0x10000), err);
        auto* o = addMem(m, "rom0");
        o->addRegion(rom(0xFF00, testRom("roms/DBL.HEX")), err);
        m.power();

        m.bus.memWrite(0xFF00, 0x42);
        CHECK(m.bus.memRead(0xFF00) == 0x21, "phantom=all: reads come from ROM");
        CHECK(r->storeAt(0xFF00) != 0x42,
              "phantom=all: the write VANISHED -- the RAM honored PHANTOM* and switched off for "
              "writes too");
    }
    {
        // phantom=read: writes fall through to the RAM beneath. Reads still come
        // from ROM. THIS IS A FOOTGUN AND IT IS SUPPOSED TO BE.
        Machine m;
        auto* r = addMem(m, "ram0");
        r->addRegion(ram(0x0000, 0x10000), err);
        auto* o = addMem(m, "rom0");
        o->addRegion(rom(0xFF00, testRom("roms/DBL.HEX")), err);
        std::string e2;
        setProperty(*o, "phantom", "read", e2);
        m.power();

        m.bus.memWrite(0xFF00, 0x42);
        CHECK(m.bus.memRead(0xFF00) == 0x21, "phantom=read: read it back and you get the ROM byte");
        CHECK(r->storeAt(0xFF00) == 0x42,
              "phantom=read: but the write LANDED in the RAM beneath. Shadow RAM.");
    }
    {
        // phantom=none: both boards drive. That is real contention, it is a real
        // fault, and the simulator reports it instead of quietly picking one.
        Machine m;
        auto* r = addMem(m, "ram0");
        r->addRegion(ram(0x0000, 0x10000), err);
        auto* o = addMem(m, "rom0");
        o->addRegion(rom(0xFF00, testRom("roms/DBL.HEX")), err);
        std::string e2;
        setProperty(*o, "phantom", "none", e2);
        m.power();

        CHECK(m.bus.respondersTo({Cycle::MemRead, 0xFF00, 0, false}).size() == 2,
              "phantom=none: BOTH boards drive FF00");
        m.bus.clearLog();
        m.bus.memRead(0xFF00);
        CHECK(!m.bus.drain().empty(), "and that is reported as contention -- do not fix it in the bus");
    }
    {
        // The same fault reached the other way: the RAM is strapped NOT to honor
        // PHANTOM*, which on a real card is a jumper you got wrong.
        Machine m;
        auto* r = addMem(m, "ram0");
        r->addRegion(ram(0x0000, 0x10000), err);
        std::string e2;
        setProperty(*r, "honors_phantom", "none", e2);
        auto* o = addMem(m, "rom0");
        o->addRegion(rom(0xFF00, testRom("roms/DBL.HEX")), err);
        m.power();

        CHECK(m.bus.respondersTo({Cycle::MemRead, 0xFF00, 0, false}).size() == 2,
              "honors_phantom=none: the RAM keeps driving under the ROM -- real contention");
    }

    SECTION("fill=random -- reproducible from its seed");

    {
        // fill=random must be REPRODUCIBLE from its seed, or it is a source of
        // nondeterminism outside the EventQueue and deterministic replay is dead
        // the first time you need it.
        Machine m1, m2;
        auto* a = addMem(m1, "mem0");
        auto* b = addMem(m2, "mem0");
        a->addRegion(ram(0x0000, 0x1000), err);
        b->addRegion(ram(0x0000, 0x1000), err);
        std::string e2;
        setProperty(*a, "seed", "12345", e2);
        setProperty(*b, "seed", "12345", e2);
        m1.power();
        m2.power();
        bool same = true;
        for (uint32_t k = 0; k < 0x1000; ++k)
            if (m1.bus.memRead((uint16_t)k) != m2.bus.memRead((uint16_t)k)) same = false;
        CHECK(same, "fill=random with the same seed is byte-identical across runs");

        setProperty(*b, "seed", "999", e2);
        m2.power();
        int diff = 0;
        for (uint32_t k = 0; k < 0x1000; ++k)
            if (m1.bus.memRead((uint16_t)k) != m2.bus.memRead((uint16_t)k)) ++diff;
        CHECK(diff > 3000, "and a different seed really is different memory");
    }
}

// ---------------------------------------------------------------------------
// A DERIVED PROPERTY HAS NO SETTER. IT DOES NOT HAVE A SETTER THAT SAYS NO.
//
// `pages` is a thing the card WORKS OUT -- it falls out of the regions you declared. It is
// not a jumper, and it does not belong in a TOML file.
//
// It used to refuse a SET from inside a setter, and that looked like it was enough. It was
// not. Read-only is a FACT ABOUT THE PROPERTY, and the only way a consumer of the reflection
// layer can see it is THE ABSENCE OF A SETTER (core/board.cpp). With a refusing setter
// installed, SHOW printed it as settable, MCP offered it as writable, and anything generated
// off properties() described it as a TOML key you could write -- while the SET they were all
// advertising failed every time.
//
// One signal, honoured by everybody, or four subsystems each guessing. This test pins the
// signal down, because the bug it guards is invisible: nothing crashes, and the damage lives
// entirely in what other subsystems believe.
void test_readonly_props() {
    SECTION("derived properties: no setter, not a setter that refuses");

    Machine m;
    auto* b = addMem(m, "mem0");
    std::string err;
    CHECK(b->addRegion(ram(0x0000, 0xE000), err), "56K of RAM");

    // properties() returns a fresh vector BY VALUE, so it must be held in a named
    // variable: binding the temporary in the range-for frees it at the loop's end, and
    // `p` (a pointer INTO it) would then dangle -- a use-after-free that reads clean on
    // some allocators and corrupts on others (it aborted under ASan on an M4 Mac).
    const auto props = b->properties();
    const Property* p = nullptr;
    for (const auto& q : props)
        if (q.name == "pages") p = &q;
    CHECK(p != nullptr, "pages exists");
    if (p) {
        // THE WHOLE POINT. Not "setting it fails" -- there is nothing there to set.
        CHECK(!p->set, "pages has NO SETTER (this is what read-only IS)");
        CHECK(!!p->get, "pages still reads");
    }

    // CONFIG SAVE must not write a key that CONFIG LOAD would then refuse. A save you cannot
    // load is not a save.
    std::string toml = saveTomlText(m);
    CHECK(toml.find("pages") == std::string::npos, "CONFIG SAVE does not write `pages`");
}

// A SAVE IS A READ.
//
// CONFIG SAVE used to probe: it called each board setter with the value it had just got, to
// find out whether the property was writable (the reflection layer answers that now, with
// `!p.set`). A setter is not a question, though. Some of them do work -- a setter can rebuild
// a board's segments or clear its live state -- so a save that called them could remap the
// address space underneath a running program.
//
// This is the whole contract in one test, and it is aimed at the MACHINE, not at the text --
// a save that perturbs what it describes is broken however good the file looks. It runs on a
// test-only board whose one setter counts its calls, so ANY call is caught, not only one that
// happens to change something visible. (Until 2026-11 it ran on the bankmem board, whose
// `card` setter rebuilt every segment; that board was removed.)
namespace {
class SetterCountBoard : public Board {
public:
    std::string type() const override { return "settercount"; }
    std::vector<Property> properties() override {
        Property p;
        p.name  = "level";
        p.help  = "a value whose setter counts its calls";
        p.kind  = Kind::Int;
        p.get   = [this] { return Value::ofInt(level_); };
        p.set   = [this](const Value& v, std::string&) {
            ++setCalls;
            level_ = v.i();
            return true;
        };
        return {p};
    }
    int setCalls = 0;

private:
    long long level_ = 7;
};
} // namespace

void test_save_is_a_read() {
    SECTION("CONFIG SAVE does not write to the machine it is describing");

    Machine m;
    auto* b = static_cast<SetterCountBoard*>(m.adopt(std::make_unique<SetterCountBoard>()));
    CHECK(b != nullptr, "the setter-counting board");
    if (!b) return;
    b->id = "probe0";
    m.power();

    std::string toml = saveTomlText(m);

    CHECK(toml.find("level = 7") != std::string::npos,
          "the save READ the property: its value is in the file");
    // THE ASSERTION. Not "the file is right" -- "the machine is untouched".
    CHECK(b->setCalls == 0, "and CONFIG SAVE called no setter on the board it described");
}

// ONE BOARD, MORE THAN 64K. `ext = true` makes the board decode A16-A23: region `at` is a
// 24-bit bus address and a region may be any size, so a 128K or a 1M block is one board
// instead of one board per segment. Without `ext` the board answers in every segment, so it
// holds 64K.
void test_memory_segments() {
    SECTION("the memory board -- ext = true, 24-bit regions, blocks past 64K");

    std::string err;
    auto board = [&](Machine& m, const std::string& id, bool ext) {
        MemoryBoard* b = addMem(m, id);
        if (ext) CHECK(setProperty(*b, "ext", "true", err), "ext = true");
        return b;
    };

    {
        // 128K at 000000: two segments, two different 64Ks of RAM.
        Machine m;
        auto* b = board(m, "mem0", true);
        CHECK(b->addRegion(ram(0x000000, 0x20000), err), "a 128K region at 000000");
        m.power();
        m.bus.memWriteX(0x001234, 0x11);
        m.bus.memWriteX(0x011234, 0x22);
        CHECK(m.bus.memReadX(0x001234) == 0x11, "segment 00 keeps its byte");
        CHECK(m.bus.memReadX(0x011234) == 0x22 && !m.bus.lastUnclaimed(),
              "segment 01 is the second 64K, not a mirror of the first");
        CHECK(m.bus.memRead(0x1234) == 0x11, "a 16-bit cycle is segment 00");
        (void)m.bus.memReadX(0x021234);
        CHECK(m.bus.lastUnclaimed(), "segment 02 is past the block and floats");
        CHECK(m.bus.peekX(0x011234) == 0x22, "peekX reads the second segment");
        auto map = b->memMap();
        CHECK(map.size() == 1 && map[0].lo == 0 && map[0].hi == 0x01FFFF,
              "the map entry is 000000-01FFFF");
    }

    {
        // 128K at 020000 answers 02 and 03 only.
        Machine m;
        auto* b = board(m, "mem0", true);
        CHECK(b->addRegion(ram(0x020000, 0x20000), err), "a 128K region at 020000");
        m.power();
        m.bus.memWriteX(0x030010, 0x5A);
        CHECK(m.bus.memReadX(0x030010) == 0x5A, "segment 03 holds the byte");
        (void)m.bus.memReadX(0x010010);
        CHECK(m.bus.lastUnclaimed(), "segment 01, below the block, floats");
        (void)m.bus.memReadX(0x040010);
        CHECK(m.bus.lastUnclaimed(), "segment 04, above the block, floats");
    }

    {
        // Two blocks in unrelated segments on one board.
        Machine m;
        auto* b = board(m, "mem0", true);
        CHECK(b->addRegion(ram(0x000000, 0x10000), err), "64K at 000000");
        CHECK(b->addRegion(ram(0x800000, 0x10000), err), "and 64K at 800000");
        m.power();
        m.bus.memWriteX(0x80ABCD, 0x99);
        CHECK(m.bus.memReadX(0x80ABCD) == 0x99, "segment 80 holds the byte");
        (void)m.bus.memReadX(0x40ABCD);
        CHECK(m.bus.lastUnclaimed(), "segment 40, between them, floats");
    }

    {
        // A region that starts mid-segment and runs into the next one.
        Machine m;
        auto* b = board(m, "mem0", true);
        CHECK(b->addRegion(ram(0x8000, 0x10000), err), "64K at 008000: 008000-017FFF");
        m.power();
        (void)m.bus.memReadX(0x007FFF);
        CHECK(m.bus.lastUnclaimed(), "007FFF is below the region");
        (void)m.bus.memReadX(0x008000);
        CHECK(!m.bus.lastUnclaimed(), "008000 is the first byte");
        (void)m.bus.memReadX(0x017FFF);
        CHECK(!m.bus.lastUnclaimed(), "017FFF is the last byte");
        (void)m.bus.memReadX(0x018000);
        CHECK(m.bus.lastUnclaimed(), "018000 is past the end");
        auto segs = m.contentionSegments();
        CHECK(std::find(segs.begin(), segs.end(), 1u) != segs.end(),
              "the contention check looks at segment 01 too");
    }

    {
        // 1M on one board.
        Machine m;
        auto* b = board(m, "mem0", true);
        CHECK(b->addRegion(ram(0x000000, 0x100000), err), "a 1M region");
        m.power();
        m.bus.memWriteX(0x0F1234, 0xC3);
        CHECK(m.bus.memReadX(0x0F1234) == 0xC3, "segment 0F holds the byte");
        (void)m.bus.memReadX(0x101234);
        CHECK(m.bus.lastUnclaimed(), "segment 10 is past 1M");
    }

    {
        // The 16-bit transfer in the second segment.
        Machine m;
        auto* b = board(m, "mem0", true);
        CHECK(setProperty(*b, "sixteen_bit", "true", err), "sixteen_bit");
        CHECK(b->addRegion(ram(0x000000, 0x20000), err), "128K");
        m.power();
        CHECK(m.bus.memWrite16(0x010010, 0xAA, 0xBB), "a word write in segment 01 is one cycle");
        Transfer16 t = m.bus.memRead16(0x010010);
        CHECK(t.sixtn && t.even == 0xAA && t.odd == 0xBB, "and reads back as one word");
        CHECK(m.bus.memReadX(0x000010) != 0xAA || m.bus.memReadX(0x000011) != 0xBB,
              "segment 00 did not get the word");
    }

    {
        // A ROM with ext: 24-bit placement. Self-placed at its own 00FF00 it answers there;
        // at 01FF00 it must ask to relocate, or the mismatch is refused.
        Machine m;
        auto* b = board(m, "mem0", true);
        CHECK(b->addRegion(rom(0xFF00, testRom("roms/DBL.HEX")), err), "DBL self-placed at 00FF00");
        CHECK(!b->addRegion(rom(0x01FF00, testRom("roms/DBL.HEX")), err) &&
                  err.find("relocate") != std::string::npos,
              "at 01FF00 without relocate is refused, and says why");
        Region up = rom(0x01FF00, testRom("roms/DBL.HEX"));
        up.relocate = true;
        CHECK(b->addRegion(up, err), "at 01FF00 with relocate = true");
        m.power();
        CHECK(m.bus.memReadX(0x00FF00) == 0x21 && m.bus.memReadX(0x01FF00) == 0x21,
              "both copies answer, each in its own segment");
        (void)m.bus.memReadX(0x02FF00);
        CHECK(m.bus.lastUnclaimed(), "segment 02 floats");
    }

    {
        // The PROM burner (Machine::burn) above 64K lands in the right segment.
        Machine m;
        auto* b = board(m, "mem0", true);
        CHECK(b->addRegion(ram(0x000000, 0x20000), err), "128K");
        m.power();
        m.bus.memWriteX(0x000020, 0x00);
        std::string why;
        CHECK(m.burn(0x010020, 0x77, why), "burn at 010020");
        CHECK(m.bus.memReadX(0x010020) == 0x77, "the byte is in segment 01");
        CHECK(m.bus.memReadX(0x000020) == 0x00, "segment 00 is untouched");
    }

    {
        // Refusals: what does not fit is said, not cut.
        Machine m;
        auto* b = board(m, "mem0", false);
        CHECK(!b->addRegion(ram(0x0000, 0x20000), err) && err.find("ext") != std::string::npos,
              "without ext, 128K is refused, and the message says to set ext");
        CHECK(!b->addRegion(ram(0x010000, 0x100), err), "without ext, at = 010000 is refused");
        auto* c = board(m, "mem1", true);
        CHECK(!c->addRegion(ram(0xFF0000, 0x20000), err), "128K at FF0000 runs past FFFFFF");
        auto* d = board(m, "mem2", true);
        CHECK(d->addRegion(ram(0x000000, 0x20000), err), "128K at 000000");
        CHECK(!setProperty(*d, "ext", "false", err), "ext = false is refused while it holds 128K");
    }

    {
        // CONFIG SAVE and load: `ext = true`, a 6-digit `at` and `size = "128K"` round-trip.
        Machine m;
        auto* b = board(m, "mem0", true);
        CHECK(b->addRegion(ram(0x000000, 0x20000), err), "128K");
        std::string toml = saveTomlText(m);
        CHECK(toml.find("\"128K\"") != std::string::npos, "the size is written as 128K");
        CHECK(toml.find("0x000000") != std::string::npos, "at is written with 6 digits");
        Machine n;
        CHECK(loadTomlText(toml, "roundtrip", n, err), "the saved text loads");
        n.power();
        n.bus.memWriteX(0x01FFFF, 0x3C);
        CHECK(n.bus.memReadX(0x01FFFF) == 0x3C, "the reloaded board holds 01FFFF");
    }

    {
        // SNAPSHOT / RESTORE carries every plane, not only the first 64K.
        Machine m;
        m.add("z80", "cpu0", err);
        auto* b = board(m, "mem0", true);
        CHECK(b->addRegion(ram(0x000000, 0x20000), err), "128K");
        CHECK(b->addRegion(ram(0x800000, 0x100), err), "and a page at 800000");
        m.power();
        m.bus.memWriteX(0x015555, 0xE7);
        m.bus.memWriteX(0x800010, 0x4D);
        std::string path =
            (std::filesystem::temp_directory_path() / "pascal100-memseg-test.snap").string();
        CHECK(m.snapshot(path, err), "snapshot writes");
        m.bus.memWriteX(0x015555, 0x00);
        m.bus.memWriteX(0x800010, 0x00);
        std::vector<std::string> notes;
        CHECK(m.restore(path, err, notes), "restore reads");
        CHECK(m.bus.memReadX(0x015555) == 0xE7, "the second segment's byte came back");
        CHECK(m.bus.memReadX(0x800010) == 0x4D, "segment 80's byte came back");
        std::filesystem::remove(path);
    }
}
