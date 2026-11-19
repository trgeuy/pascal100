// The IEEE-696 bus type (Stage 2, Task 2): A16-A23, segment tables, 16-bit I/O addresses,
// sXTRQ*/SIXTN* and the byte-serial fall-back, the address translator, and the bus-type
// refusal. reference/IEEE_696_1983.md is the source for every rule tested here.
#include "test.h"

#include "boards/registry.h"
#include "boards/s100-memory.h"
#include "config/toml.h"
#include "core/machine.h"
#include "cli/monitor.h"
#include "core/hex.h"
#include "mcp/server.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include <vector>

using namespace altair;

namespace {

// A memory board that DECODES A16-A23: it answers only in segment `seg`. With
// `sixteen` it answers SIXTN* and moves both bytes in one cycle. Counts what it is asked.
struct SegBoard : Board {
    uint8_t seg = 1;
    bool sixteen = false;
    std::vector<uint8_t> mem = std::vector<uint8_t>(0x10000, 0);
    int reads = 0, writes = 0, reads16 = 0, writes16 = 0;

    std::string type() const override { return "segtest"; }
    std::vector<Property> properties() override { return {}; }
    bool decodes(const BusCycle& c) const override {
        return (c.type == Cycle::MemRead || c.type == Cycle::MemWrite) && c.ext == seg;
    }
    bool decodesExtended() const override { return true; }
    uint8_t read(const BusCycle& c) override { ++reads; return mem[c.addr]; }
    void write(const BusCycle& c) override { ++writes; mem[c.addr] = c.data; }
    bool peekX(uint32_t phys, uint8_t& out) const override {
        if ((phys >> 16) != seg) return false;
        out = mem[phys & 0xFFFF];
        return true;
    }
    bool answersSixtn(const BusCycle&) const override { return sixteen; }
    void read16(const BusCycle& c, uint8_t& even, uint8_t& odd) override {
        ++reads16;
        even = mem[c.addr];
        odd  = mem[(uint16_t)(c.addr + 1)];
    }
    void write16(const BusCycle& c) override {
        ++writes16;
        mem[c.addr] = c.data;
        mem[(uint16_t)(c.addr + 1)] = c.dataHi;
    }
    void move(uint8_t s) { seg = s; decodeChanged(); }
};

// An I/O board at port 34 that records the full address of the last cycle it answered.
struct IoProbe : Board {
    uint16_t last = 0xEEEE;
    std::string type() const override { return "ioprobe"; }
    std::vector<Property> properties() override { return {}; }
    bool decodes(const BusCycle& c) const override {
        return (c.type == Cycle::IoRead || c.type == Cycle::IoWrite) && c.port() == 0x34;
    }
    uint8_t read(const BusCycle& c) override { last = c.addr; return 0x99; }
    void write(const BusCycle& c) override { last = c.addr; }
};

struct Plus64K : AddressTranslator {
    uint32_t translate(uint16_t a, Cycle) override { return 0x010000u | a; }
};

MemoryBoard* ram64(Machine& m, const char* id = "mem0") {
    std::string err;
    auto* mb = dynamic_cast<MemoryBoard*>(m.add("memory", id, err));
    Region r;
    r.kind = RegionKind::Ram;
    r.at = 0;
    r.size = 0x10000;
    mb->addRegion(r, err);
    setProperty(*mb, "fill", "zero", err);
    return mb;
}

// A 16-bit DMA master for the breakpoint test: when granted, it moves one word with
// memWrite16 and lets go of pHOLD. A harness board, not a model of any card.
struct Word16Dma : Board {
    Word16Dma() { mover.d = this; }
    void start(uint32_t at) {
        at_   = at;
        hold_ = true;
        holdChanged();
    }
    bool sixtn = false;
    std::string type() const override { return "test-dma16"; }
    std::vector<Property> properties() override { return {}; }
    bool       requestsBus() const override { return hold_; }
    BusMaster* busMaster() override { return &mover; }
    struct Mover : BusMaster {
        Word16Dma* d = nullptr;
        StepResult step(Bus& bus) override {
            d->sixtn = bus.memWrite16(d->at_, 0xC3, 0xD4);
            d->hold_ = false;
            d->holdChanged();
            return {8, RunStatus::Ok};
        }
    } mover;
    uint32_t at_   = 0;
    bool     hold_ = false;
};

struct Seen {
    std::vector<BusCycle> cycles;
    int h = 0;
    Bus* bus = nullptr;
    explicit Seen(Bus& b) : bus(&b) {
        h = b.observe([this](const BusCycle& c) { cycles.push_back(c); });
    }
    ~Seen() { bus->unobserve(h); }
};

} // namespace

void test_ieee696() {
    SECTION("IEEE 696: A16-A23 and the segment tables");

    {
        // An 8-bit board that reads only `addr` ALIASES into every 64K segment -- the
        // standard's behaviour for a slave that does not decode the extended address.
        Machine m;
        ram64(m);
        m.power();
        m.bus.setVerify(true);
        m.bus.memWriteX(0x5A0123, 0x77);
        CHECK(m.bus.memRead(0x0123) == 0x77, "a write in segment 5A lands in the aliasing board");
        CHECK(m.bus.memReadX(0xFF0123) == 0x77, "...and reads back in segment FF");
        CHECK(m.bus.peekX(0x010123) == 0x77, "peekX sees it in segment 01 too");
    }

    {
        // A board that decodes A16-A23 answers in its own segment only.
        Machine m;
        auto* sb = new SegBoard;
        sb->id = "seg0";
        m.adopt(std::unique_ptr<Board>(sb));
        m.power();
        m.bus.setVerify(true);  // every segment's table checked against the exact scan
        m.bus.memWriteX(0x010200, 0x42);
        CHECK(sb->mem[0x0200] == 0x42 && sb->writes == 1, "segment 01 reaches the board");
        CHECK(m.bus.memReadX(0x020200) == 0xFF && m.bus.lastUnclaimed(),
              "segment 02 floats -- nobody decodes it");
        CHECK(m.bus.memRead(0x0200) == 0xFF, "and segment 0 floats too");
        CHECK(m.bus.peekX(0x010200) == 0x42 && m.bus.peekX(0x030200) == 0xFF,
              "peekX asks the board for its own segment only");

        // decodeChanged() drops the segment tables with the rest.
        sb->move(2);
        CHECK(m.bus.memReadX(0x020200) == 0x42, "after a decode change, segment 02 answers");
        CHECK(m.bus.memReadX(0x010200) == 0xFF, "...and segment 01 has let go");

        Seen s(m.bus);
        m.bus.memReadX(0x020200);
        CHECK(s.cycles.size() == 1 && s.cycles[0].ext == 0x02 && s.cycles[0].addr == 0x0200,
              "the observer sees A16-A23 in BusCycle::ext");
    }

    SECTION("IEEE 696: 16-bit I/O addresses");

    {
        Machine m;
        auto* io = new IoProbe;
        io->id = "io0";
        m.adopt(std::unique_ptr<Board>(io));
        m.bus.setVerify(true);  // the I/O decode by A0-A7 is checked against the full address
        CHECK(m.bus.ioRead(0x1234) == 0x99 && io->last == 0x1234,
              "IEEE: the board decodes port 34 and sees A0-A15 = 1234");
        m.bus.ioWrite(0xAB34, 0);
        CHECK(io->last == 0xAB34, "IEEE: an OUT carries A8-A15 too");
    }

    {
        // The CPU cores drive A8-A15 as the chips do.
        auto rig = [](const char* cpu, std::initializer_list<uint8_t> code, int steps) {
            Machine m;
            std::string err;
            ram64(m);
            auto* io = new IoProbe;
            io->id = "io0";
            m.adopt(std::unique_ptr<Board>(io));
            m.add(cpu, "cpu0", err);
            m.power();
            uint16_t a = 0;
            for (uint8_t b : code) m.bus.memWrite(a++, b);
            m.reset(Reset::PowerOn);
            std::vector<uint16_t> seen;
            for (int i = 0; i < steps; ++i) {
                io->last = 0xEEEE;
                m.master()->step(m.bus);
                seen.push_back(io->last);
            }
            return seen;
        };

        // LD A,12 / IN A,(34) / LD A,56 / OUT (34),A
        auto z = rig("z80", {0x3E, 0x12, 0xDB, 0x34, 0x3E, 0x56, 0xD3, 0x34}, 4);
        CHECK(z[1] == 0x1234, "Z80 IN A,(n): A on A8-A15");
        CHECK(z[3] == 0x5634, "Z80 OUT (n),A: A on A8-A15");

        // LD BC,5634 / IN A,(C) / OUT (C),A / LD HL,8000 / INI / OUTI
        auto zc = rig("z80",
                      {0x01, 0x34, 0x56, 0xED, 0x78, 0xED, 0x79, 0x21, 0x00, 0x80, 0xED, 0xA2,
                       0xED, 0xA3},
                      6);
        CHECK(zc[1] == 0x5634, "Z80 IN A,(C): B on A8-A15");
        CHECK(zc[2] == 0x5634, "Z80 OUT (C),A: B on A8-A15");
        CHECK(zc[4] == 0x5634, "Z80 INI: B BEFORE the decrement on A8-A15");
        CHECK(zc[5] == 0x5434, "Z80 OUTI: B AFTER the decrement on A8-A15 (55 -> 54)");

        // IN 34 / OUT 34 -- the 8080 and 8085 copy the port onto A8-A15.
        for (const char* cpu : {"8085"}) {  // pascal100: no 8080 core
            auto e = rig(cpu, {0xDB, 0x34, 0xD3, 0x34}, 2);
            CHECK(e[0] == 0x3434 && e[1] == 0x3434,
                  (std::string(cpu) + ": the port on both halves, 3434").c_str());
        }
    }

    SECTION("IEEE 696: sXTRQ*/SIXTN* and the byte-serial fall-back");

    {
        // A 16-bit slave: ONE cycle, both bytes, xtrq set.
        Machine m;
        auto* sb = new SegBoard;
        sb->id = "seg0";
        sb->sixteen = true;
        m.adopt(std::unique_ptr<Board>(sb));
        m.bus.setVerify(true);
        sb->mem[0x0300] = 0xA1;
        sb->mem[0x0301] = 0xB2;
        Seen s(m.bus);
        Transfer16 r = m.bus.memRead16(0x010300);
        CHECK(r.sixtn && r.even == 0xA1 && r.odd == 0xB2,
              "SIXTN*: one 16-bit read, even byte A1, odd byte B2");
        CHECK(sb->reads16 == 1 && sb->reads == 0, "...through read16(), not two read()s");
        CHECK(s.cycles.size() == 1 && s.cycles[0].xtrq && s.cycles[0].data == 0xA1 &&
                  s.cycles[0].dataHi == 0xB2 && s.cycles[0].ext == 1,
              "the observer sees ONE cycle with sXTRQ*, ED and OD");

        s.cycles.clear();
        CHECK(m.bus.memWrite16(0x010310, 0xC3, 0xD4), "SIXTN*: one 16-bit write");
        CHECK(sb->mem[0x0310] == 0xC3 && sb->mem[0x0311] == 0xD4 && sb->writes16 == 1,
              "even byte at the even address, odd byte at the odd one");
        CHECK(s.cycles.size() == 1 && s.cycles[0].xtrq, "...in one observed cycle");

        CHECK(!m.bus.memRead16(0x010301).sixtn, "an odd address gets no 16-bit cycle");
    }

    {
        // An 8-bit slave: no SIXTN*, NOTHING driven; the master goes byte-serial.
        Machine m;
        ram64(m);
        m.power();
        m.bus.setVerify(true);
        m.bus.memWrite(0x0400, 0x11);
        m.bus.memWrite(0x0401, 0x22);
        Seen s(m.bus);
        Transfer16 r = m.bus.memRead16(0x000400);
        CHECK(!r.sixtn, "an 8-bit board does not answer SIXTN*");
        CHECK(s.cycles.empty(), "...and no cycle ran: nothing driven, nothing observed");

        r = m.bus.byteSerialRead16(0x000400, /*evenFirst=*/true);
        CHECK(r.even == 0x11 && r.odd == 0x22, "byte-serial: the two bytes assembled");
        CHECK(s.cycles.size() == 2 && s.cycles[0].addr == 0x0400 && s.cycles[1].addr == 0x0401 &&
                  !s.cycles[0].xtrq && !s.cycles[1].xtrq,
              "byte-serial, even first: two byte cycles, 0400 then 0401");
        s.cycles.clear();
        m.bus.byteSerialRead16(0x000400, /*evenFirst=*/false);
        CHECK(s.cycles.size() == 2 && s.cycles[0].addr == 0x0401 && s.cycles[1].addr == 0x0400,
              "byte-serial, odd first: 0401 then 0400 -- the master's order");

        s.cycles.clear();
        CHECK(!m.bus.memWrite16(0x000500, 0x33, 0x44), "no SIXTN* on a write either");
        CHECK(s.cycles.empty() && m.bus.memRead(0x0500) == 0, "...and nothing was written");
        s.cycles.clear();
        m.bus.byteSerialWrite16(0x000500, 0x33, 0x44, /*evenFirst=*/false);
        CHECK(s.cycles.size() == 2 && s.cycles[0].addr == 0x0501 && s.cycles[1].addr == 0x0500,
              "byte-serial write, odd first");
        CHECK(m.bus.memRead(0x0500) == 0x33 && m.bus.memRead(0x0501) == 0x44,
              "byte-serial write: both bytes landed");
        CHECK(!m.bus.memRead16(0x050000).sixtn, "the floating bus answers no SIXTN* either");
    }

    SECTION("the address translator");

    {
        Plus64K t;
        Machine m;
        auto* sb = new SegBoard;  // segment 1 only
        sb->id = "seg0";
        m.adopt(std::unique_ptr<Board>(sb));
        m.bus.setVerify(true);
        m.bus.setTranslator(&t);
        m.bus.memWrite(0x0600, 0x66);
        CHECK(sb->mem[0x0600] == 0x66, "IEEE: logical 0600 -> physical 010600");
        CHECK(m.bus.memRead(0x0600) == 0x66, "...and back");
        CHECK(m.bus.peek(0x0600) == 0xFF, "peek is physical: segment 0 is empty");
        CHECK(m.bus.memReadX(0x000600) == 0xFF, "memReadX is physical too");
        m.bus.setTranslator(nullptr);
        CHECK(m.bus.memRead(0x0600) == 0xFF, "with no translator, 0600 is physical again");
    }

    {
        Plus64K t;
        Machine m;
        ram64(m);
        m.power();
        m.bus.setTranslator(&t);
        m.bus.memWrite(0x0700, 0x70);
        m.bus.setTranslator(nullptr);
        CHECK(m.bus.memRead(0x0700) == 0x70,
              "a board that ignores A16-A23 takes the translated 010700 at its 0700");
    }

    {
        // TranslatorScope puts the old translator back even when a cycle breakpoint
        // unwinds out of the step.
        Plus64K t;
        Machine m;
        ram64(m);
        m.power();
        m.bus.setPreAccessVeto([](const BusCycle&) { return true; });
        bool threw = false;
        try {
            TranslatorScope scope(m.bus, &t);
            CHECK(m.bus.translator() == &t, "inside the scope the translator is set");
            m.bus.memRead(0x0100);
        } catch (const CycleBreakBefore&) {
            threw = true;
        }
        m.bus.clearPreAccessVeto();
        CHECK(threw && m.bus.translator() == nullptr,
              "after a CycleBreakBefore unwinds, the translator is cleared again");
    }

    {
        // With no SIXTN*, the breakpoint veto sees only the byte cycles that run.
        Machine m;
        ram64(m);
        m.power();
        int asked = 0;
        m.bus.setPreAccessVeto([&](const BusCycle&) { ++asked; return false; });
        if (!m.bus.memRead16(0x000400).sixtn) m.bus.byteSerialRead16(0x000400, true);
        m.bus.clearPreAccessVeto();
        CHECK(asked == 2, "no SIXTN*: the veto is asked twice (the two byte cycles), not three times");
    }

    SECTION("the bus type: refusal, the loader, CONFIG SAVE");

    {
        Machine m;
        std::string err;
        // pascal100 2026-11-09: fp and turnkey (the only altair-only boards) are gone, so the
        // refusal has nothing left to refuse; the second bus type itself goes in step 3.
        CHECK(m.add("memory", "mem0", err) != nullptr, "memory goes on either bus");
    }

    {
        Machine m;
        std::string err;
        // 2026-11-18: the `bus` key is gone. IEEE 696 is the only bus, so every value --
        // the old "ieee696" too -- is refused, and the message says the line can go.
        for (const char* v : {"ieee696", "altair", "s100"}) {
            bool ok = loadTomlText(std::string("[machine]\nname = \"t\"\nbus = \"") + v + "\"\n",
                                   "t.toml", m, err);
            CHECK(!ok && err.find("line 3") != std::string::npos &&
                      err.find("pascal100 has one bus, IEEE 696") != std::string::npos &&
                      err.find("Delete this line") != std::string::npos,
                  ("bus = " + std::string(v) + " is refused and says why: " + err).c_str());
        }

        bool ok = loadTomlText("[machine]\nname = \"t\"\n"
                               "[[board]]\ntype = \"memory\"\nid = \"mem0\"\n",
                               "t.toml", m, err);
        CHECK(ok, ("no bus key: loads: " + err).c_str());
        std::string saved = saveTomlText(m);
        CHECK(saved.find("bus") == std::string::npos,
              ("CONFIG SAVE writes no bus key: " + saved).c_str());
        Machine back;
        ok = loadTomlText(saved, "saved.toml", back, err);
        CHECK(ok, ("...and it round-trips: " + err).c_str());

        ok = loadTomlText("[machine]\nname = \"t\"\nbsu = \"x\"\n", "t.toml", m, err);
        CHECK(!ok && err.find("Valid keys: base name startup") != std::string::npos,
              ("an unknown key lists the valid keys, and bus is not one: " + err).c_str());
    }

    SECTION("memory: ext and sixteen_bit");

    {
        Machine m;
        auto* mb = ram64(m);
        std::string err;
        int listed = 0;
        for (const auto& p : mb->properties())
            if (p.name == "ext" || p.name == "sixteen_bit") ++listed;
        CHECK(listed == 2, "memory lists ext and sixteen_bit");
        CHECK(!setProperty(*mb, "nosuch", "1", err) &&
                  err.find("has no property 'nosuch'") != std::string::npos,
              "an unknown key still gets the ordinary message");
    }

    {
        Machine m;
        auto* mb = ram64(m);
        m.power();
        std::string err;
        CHECK(setProperty(*mb, "ext", "true", err), ("IEEE: ext = true: " + err).c_str());
        m.bus.setVerify(true);
        m.bus.memWriteX(0x000300, 0xAB);
        CHECK(m.bus.memReadX(0x000300) == 0xAB, "ext: the region at 0000 is segment 00");
        CHECK(m.bus.memReadX(0x020300) == 0xFF && m.bus.memReadX(0x030300) == 0xFF,
              "ext: segments 02 and 03 float");
        CHECK(m.bus.peekX(0x000300) == 0xAB && m.bus.peekX(0x010300) == 0xFF,
              "peekX follows ext");
        CHECK(!setProperty(*mb, "ext", "02", err), "ext is true/false: 02 is refused");
        CHECK(!setProperty(*mb, "ext", "zz", err), "ext zz is refused");
        CHECK(setProperty(*mb, "ext", "FALSE", err), "ext false (any case) goes back to aliasing");
        CHECK(m.bus.memReadX(0x7F0300) == 0xAB, "ext false: every segment again");

        CHECK(!m.bus.memRead16(0x000300).sixtn, "sixteen_bit off: no SIXTN*");
        CHECK(setProperty(*mb, "sixteen_bit", "true", err), "sixteen_bit = true");
        m.bus.memWrite(0x0301, 0xCD);
        Transfer16 r = m.bus.memRead16(0x000300);
        CHECK(r.sixtn && r.even == 0xAB && r.odd == 0xCD,
              "sixteen_bit: one cycle, even byte AB at 0300, odd byte CD at 0301");
        CHECK(m.bus.memWrite16(0x000400, 0x12, 0x34) && m.bus.memRead(0x0400) == 0x12 &&
                  m.bus.memRead(0x0401) == 0x34,
              "sixteen_bit: a word write lands even/odd");

        std::vector<MapEntry> map = mb->memMap();
        CHECK(!map.empty() && map[0].note.find("16-bit") != std::string::npos,
              "SHOW BUS MAP notes 16-bit");
        setProperty(*mb, "ext", "true", err);
        map = mb->memMap();
        CHECK(!map.empty() && map[0].note.find("ext") != std::string::npos,
              "SHOW BUS MAP notes ext");
    }

    {
        // A ROM region in another segment does not pull PHANTOM* here.
        Machine m;
        std::string err;
        auto* rom = dynamic_cast<MemoryBoard*>(m.add("memory", "rom0", err));
        setProperty(*rom, "ext", "true", err);
        Region x;
        x.kind = RegionKind::Rom;
        x.at = 0x01FF00;
        x.relocate = true;  // the DBL image places itself at 00FF00; move it to segment 01
        x.mount = testRom("roms/DBL.HEX");
        rom->addRegion(x, err);
        ram64(m, "ram0");
        m.power();
        BusCycle c{Cycle::MemRead, 0xFF00, 0, false};
        c.ext = 0x01;
        CHECK(rom->assertsPhantom(c), "the ROM shadows FF00 in its own segment 01");
        c.ext = 0x00;
        CHECK(!rom->assertsPhantom(c), "...and not in segment 00");
    }

    SECTION("the IEEE 696 test machines (tests/media/ieee696)");

    {
        const std::string dir = std::string(ALTAIR_SOURCE_DIR) + "/tests/media/ieee696/";
        Machine m;
        std::string err;
        CHECK(loadToml(dir + "cpm22n-alias64k.toml", m, err),
              ("cpm22n-alias64k.toml loads: " + err).c_str());
        m.bus.setVerify(true);
        m.bus.ioWrite(0xC3, 0x00);  // the Disk 1's boot ROM off: it answers every read while on
        m.bus.memWriteX(0x000200, 0x5E);
        CHECK(m.bus.memReadX(0x9A0200) == 0x5E, "alias64k: segment 9A is segment 00");

        // The real guest, under verify: the Disk 1 EPROM boots CP/M 2.2N with the Z80 driving
        // A8-A15 on every I/O cycle, and each cycle's cached decode (built with A8-A15 = 0) is
        // re-derived against the full address. A board that decoded A8-A15 aborts here; the
        // CLI acceptance test (tests/acceptance/ieee696.cmake) cannot see that, because the
        // fast path never asks decodes() again.
        m.reset(Reset::PowerOn);
        m.cpu()->setPc(0x0000);
        m.debug.run(3000000);
        bool signon = false;
        for (uint32_t a = 0; a < 0x10000 - 8 && !signon; ++a)
            if (m.bus.peek((uint16_t)a) == 'C' && m.bus.peek((uint16_t)(a + 1)) == 'o' &&
                m.bus.peek((uint16_t)(a + 2)) == 'm' && m.bus.peek((uint16_t)(a + 3)) == 'p' &&
                m.bus.peek((uint16_t)(a + 4)) == 'u' && m.bus.peek((uint16_t)(a + 5)) == 'P')
                signon = true;
        CHECK(signon, "alias64k: the CBIOS (with its CompuPro sign-on) is in memory after the boot");

        Machine h;
        CHECK(loadToml(dir + "cpm22n-seg128k.toml", h, err),
              ("cpm22n-seg128k.toml loads: " + err).c_str());
        h.bus.setVerify(true);
        h.bus.ioWrite(0xC3, 0x00);  // the Disk 1's boot ROM off
        h.bus.memWriteX(0x000200, 0x11);
        h.bus.memWriteX(0x010200, 0x22);
        CHECK(h.bus.memReadX(0x000200) == 0x11 && h.bus.memReadX(0x010200) == 0x22,
              "seg128k: segments 00 and 01 are separate RAM");
        CHECK(h.bus.memReadX(0x020200) == 0xFF, "seg128k: segment 02 floats");
        h.bus.memWriteX(0x010201, 0x33);
        Transfer16 r = h.bus.memRead16(0x010200);
        CHECK(r.sixtn && r.even == 0x22 && r.odd == 0x33, "seg128k: a word read in segment 01");
        std::string saved = saveTomlText(h);
        CHECK(saved.find("ext") != std::string::npos &&
                  saved.find("sixteen_bit") != std::string::npos,
              "seg128k: CONFIG SAVE writes ext and sixteen_bit");
        // CONFIG SAVE writes the disk path as the base file named it, relative to
        // tests/media/compupro, so the saved text is read as if it lived there.
        Machine back;
        CHECK(loadTomlText(saved, dir + "../compupro/saved.toml", back, err),
              ("seg128k: the saved file loads back: " + err).c_str());
        back.bus.ioWrite(0xC3, 0x00);  // the Disk 1's boot ROM off
        back.bus.memWriteX(0x000200, 0x55);
        back.bus.memWriteX(0x010200, 0x44);
        CHECK(back.bus.memReadX(0x000200) == 0x55 &&
                  back.bus.memReadX(0x010200) == 0x44 && back.bus.memRead16(0x010200).sixtn,
              "seg128k: the round trip keeps both segments and sixteen_bit");
    }

    SECTION("monitor: physical, 24-bit addresses on the IEEE 696 bus (Task 4)");

    {
        Machine m;
        Monitor mon(m);
        std::string err;
        std::ostringstream sink;
        auto run = [&](const std::string& line) {
            std::ostringstream o;
            mon.exec(line, o);
            return o.str();
        };
        auto has = [](const std::string& s, const std::string& w) {
            return s.find(w) != std::string::npos;
        };
        run("BOARDS ADD memory mem0");
        run("SET mem0 fill=zero");
        run("SET mem0 ext=true");
        run("REGION ADD mem0 type=ram at=0 size=64K");
        run("BOARDS ADD memory mem1");
        run("SET mem1 fill=zero");
        run("SET mem1 ext=true");
        run("REGION ADD mem1 type=ram at=10000 size=64K");

        std::string r = run("FILL 10100-1010F 5A");
        CHECK(has(r, "filled 010100-01010F with 5A"), ("FILL: 6-digit range: " + r).c_str());
        r = run("D 10100-1010F");
        CHECK(r.rfind("010100  5A 5A 5A", 0) == 0, ("DUMP reads segment 01: " + r).c_str());
        r = run("D 100-10F");
        CHECK(r.rfind("000100  00 00", 0) == 0, ("DUMP below 64K is 6 digits too: " + r).c_str());
        CHECK(run("D 1000000") == "address is 24 bits: 000000-FFFFFF\n", "DUMP past A23 is refused");
        CHECK(has(run("FILL FFFFF0-1000005 0"), "address is 24 bits"), "a range past A23 is refused");

        r = run("WHO 10100");
        CHECK(has(r, "010100 read  mem1"), ("WHO names the segment-01 board: " + r).c_str());
        r = run("WHO 20100");
        CHECK(has(r, "020100 read  nobody"), ("WHO: segment 02 floats: " + r).c_str());

        r = run("SEARCH 10000-1FFFF 5A 5A");
        CHECK(r.rfind("010100\n", 0) == 0, ("SEARCH finds segment 01: " + r).c_str());
        CHECK(has(run("MOVE 10100-1010F 20"), "16 bytes moved"), "MOVE from segment 01 to 00");
        CHECK(m.bus.memReadX(0x00002F) == 0x5A && m.bus.memReadX(0x010030) == 0x00,
              "MOVE wrote segment 00, not segment 01");
        CHECK(has(run("COMPARE 10100-1010F 20"), "0 difference(s)"), "COMPARE across segments");
        // The PROM burner (ROM) goes behind the bus, to the board that answers that physical
        // address: segment 01's board, not the segment 00 board at the same A0-A15.
        run("FILL 10200-10200 77 ROM");
        CHECK(m.bus.memReadX(0x010200) == 0x77 && m.bus.memReadX(0x000200) == 0x00,
              "FILL ... ROM burns segment 01, not segment 00");

        r = run("SHOW BUS MAP");
        CHECK(has(r, "000000-00FFFF  mem0") && has(r, "010000-01FFFF  mem1"),
              ("SHOW BUS MAP lists both segments: " + r).c_str());
        CHECK(has(r, "other segments: nothing answers"), "...and says the rest float");
        CHECK(has(run("SHOW BUS CONTENTION"), "none."), "no contention between the segments");

        // SAVE writes a type 04 record above 64K; LOAD reads it back into segment 01.
        const std::string hex = (std::filesystem::temp_directory_path() / "ieee696_seg01.hex").string();
        r = run("SAVE \"" + hex + "\" 10100-1010F");
        CHECK(has(r, "saved 010100-01010F"), ("SAVE: 6-digit range: " + r).c_str());
        std::string text;
        {
            std::ifstream f(hex);
            text.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        }
        CHECK(text.rfind(":020000040001F9\n:10010000", 0) == 0,
              ("the file opens with a type 04 record for segment 01: " + text).c_str());
        run("FILL 10100-1010F 0");
        r = run("LOAD \"" + hex + "\"");
        CHECK(has(r, "(010100-01010F)") && m.bus.memReadX(0x01010F) == 0x5A &&
                  m.bus.memReadX(0x00010F) == 0x00,
              ("LOAD puts it back in segment 01: " + r).c_str());
        r = run("LOAD \"" + hex + "\" AT 20200");
        CHECK(m.bus.memReadX(0x020200) == 0xFF && has(r, "(020200-02020F)"),
              ("LOAD AT relocates by 24 bits (segment 02 has no RAM, so it floats): " + r).c_str());
        std::filesystem::remove(hex);

        // The CPU's addresses are not physical: a PC breakpoint stays 4 digits, a MEM one is 6.
        m.debug.add(BreakKind::Pc, 0x0100, 0x0100);
        m.debug.add(BreakKind::MemWrite, 0x010105, 0x010105);
        CHECK(m.debug.breakpoints().at(0).describe() == "pc     0100", "PC breakpoint: 4 digits");
        CHECK(m.debug.breakpoints().at(1).describe() == "mem w  010105",
              ("MEM breakpoint: 6 digits: " + m.debug.breakpoints().at(1).describe()).c_str());
    }

    SECTION("TRACE/HISTORY: A16-A23 and the 16-bit transfer");

    {
        std::vector<std::string> handles{"mem1"};
        Debugger::CycleRec w;
        w.type = Cycle::MemRead;
        w.addr = 0x0100;
        w.ext = 0x01;
        w.data = 0x12;
        w.dataHi = 0x34;
        w.xtrq = true;
        w.t = 7;
        w.responder = 0;
        std::string line = Debugger::formatCycle(w, handles);
        CHECK(line == "         7  MR16 010100 = 12 34   cpu      -> mem1",
              ("a 16-bit read in segment 01: [" + line + "]").c_str());
        w.xtrq = false;
        w.type = Cycle::IoWrite;
        w.addr = 0x01C2;
        line = Debugger::formatCycle(w, handles);
        CHECK(line == "         7  OUT    01C2 = 12      cpu      -> mem1",
              ("an IEEE I/O cycle shows A0-A15: [" + line + "]").c_str());
        Debugger::CycleRec a;  // a plain byte read in segment 00: still 6 digits
        a.type = Cycle::MemRead;
        a.addr = 0x0100;
        a.data = 0x12;
        a.t = 7;
        line = Debugger::formatCycle(a, handles);
        CHECK(line == "         7  MR   000100 = 12      cpu      -> --",
              ("segment 00 shows 6 digits too: [" + line + "]").c_str());
    }

    SECTION("Intel HEX: type 04 on the IEEE 696 bus");

    {
        Image img;
        img.bytes[0x00FFFE] = 1;
        img.bytes[0x00FFFF] = 2;
        img.bytes[0x010000] = 3;
        img.bytes[0x020005] = 4;
        std::string t = saveHex(img);
        CHECK(t.rfind(":02FFFE00", 0) == 0, "segment 0 first, with no type 04");
        CHECK(t.find(":020000040001F9\n:0100000003") != std::string::npos,
              ("a record never crosses 64K; 04 names segment 01: " + t).c_str());
        CHECK(t.find(":020000040002F8\n:0100050004") != std::string::npos, "04 for segment 02");
        Image back;
        std::string err;
        std::vector<uint8_t> raw(t.begin(), t.end());
        CHECK(loadHex(raw, back, err, 0xFFFFFF) && back.bytes == img.bytes,
              ("the 24-bit file round-trips: " + err).c_str());
        Image narrow;
        CHECK(!loadHex(raw, narrow, err) && err.find("beyond 64K") != std::string::npos,
              "with the default 64K limit, type 04 above 64K is still refused");
        Image small;
        small.bytes[0x0100] = 9;
        CHECK(saveHex(small).find(":04") == std::string::npos && saveHex(small).find("0004") ==
                  std::string::npos,
              "a 64K image gets no type 04 record");
    }

    SECTION("MCP: mem_* tools take 24-bit addresses on the IEEE 696 bus");

    {
        Machine m;
        std::string err;
        Monitor mon(m);
        std::ostringstream o;
        mon.exec("BOARDS ADD memory mem1", o);
        mon.exec("SET mem1 fill=zero", o);
        mon.exec("SET mem1 ext=true", o);
        mon.exec("REGION ADD mem1 type=ram at=10000 size=64K", o);
        auto call = [](int id, const std::string& tool, const std::string& args) {
            return R"({"jsonrpc":"2.0","id":)" + std::to_string(id) +
                   R"(,"method":"tools/call","params":{"name":")" + tool +
                   R"(","arguments":)" + args + "}}\n";
        };
        std::string script =
            std::string(R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})") + "\n" +
            call(2, "mem_deposit", R"({"addr":65792,"bytes":"C3 00 F8"})") +   // 010100
            call(3, "mem_dump", R"({"lo":65792,"hi":65794})") +
            call(4, "who", R"({"addr":65792})") +
            call(5, "mem_search", R"({"lo":65536,"hi":131071,"bytes":"C3 00 F8"})") +
            call(6, "mem_fill", R"({"lo":65800,"hi":65801,"byte":17})") +
            call(7, "bus_map", "{}") +
            call(8, "mem_search", R"({"lo":0,"hi":16777215,"bytes":"C3 00 F8"})") +
            call(9, "mem_dump", R"({"lo":0,"hi":4294967295})");
        std::istringstream in(script);
        std::ostringstream out;
        runMcp(m, in, out);
        const std::string r = out.str();
        CHECK(m.bus.memReadX(0x010100) == 0xC3 && m.bus.memReadX(0x010102) == 0xF8,
              "mem_deposit wrote segment 01");
        CHECK(r.find("\"bytes\":[195,0,248]") != std::string::npos, "mem_dump read segment 01");
        CHECK(r.find("010100 read: mem1") != std::string::npos, "who: 6 digits, segment 01");
        CHECK(r.find("\"matches\":[65792]") != std::string::npos, "mem_search found 010100");
        CHECK(r.find("filled 010108-010109 with 11") != std::string::npos, "mem_fill: 6 digits");
        CHECK(r.find("\"segments\":[1]") != std::string::npos, ("bus_map names segment 01: " + r).c_str());
        size_t refused = 0;
        for (size_t at = 0; (at = r.find("one call takes at most 64K", at)) != std::string::npos; ++at)
            ++refused;
        CHECK(refused == 2, "a span over 64K is refused (mem_search 16M, mem_dump 0-FFFFFFFF)");
    }

    SECTION("code review fixes (2026-11-03)");

    {
        // A 16-bit transfer moves both bytes in one cycle: a MEM breakpoint on the odd byte
        // fires too, and one on the next word does not.
        Machine m;
        auto* mb = ram64(m);
        m.power();
        std::string err;
        CHECK(setProperty(*mb, "sixteen_bit", "true", err), "sixteen_bit = true");
        setProperty(*mb, "fill", "zero", err);  // NOPs for the CPU
        mb->power();
        m.add("z80", "cpu0", err);
        m.cpu()->reset(Reset::PowerOn);
        Word16Dma dma;
        dma.id = "dma0";
        m.bus.attach(&dma);
        m.debug.add(BreakKind::MemWrite, 0x0311, 0x0311, nullptr, BreakAction::TraceOn);
        m.debug.add(BreakKind::MemWrite, 0x0310, 0x0310, nullptr, BreakAction::TraceOn);
        m.debug.add(BreakKind::MemWrite, 0x0312, 0x0312, nullptr, BreakAction::TraceOn);
        dma.start(0x000310);
        m.debug.run(200);
        m.bus.detach(&dma);
        CHECK(dma.sixtn && m.bus.memRead(0x0311) == 0xD4, "one 16-bit DMA write");
        const auto& bps = m.debug.breakpoints();
        CHECK(bps.size() == 3 && bps[0].hits == 1 && bps[1].hits == 1 && bps[2].hits == 0,
              "16-bit write: odd byte 0311 hits, even byte 0310 hits, 0312 does not");
    }

    {
        // HEX: with the 24-bit limit a data byte past FFFFFF is refused, not wrapped into
        // segment 00. loadHex's default limit keeps the old behaviour for a type 02 base.
        std::string err;
        std::string t = ":0200000400FFFB\n:10FFF800000102030405060708090A0B0C0D0E0F81\n:00000001FF\n";
        std::vector<uint8_t> raw(t.begin(), t.end());
        Image img;
        CHECK(!loadHex(raw, img, err, 0xFFFFFF) && err.find("past FFFFFF") != std::string::npos,
              ("IEEE: data past FFFFFF is refused: " + err).c_str());
        t = ":0200000400FFFB\n:10FFF000000102030405060708090A0B0C0D0E0F89\n:00000001FF\n";
        raw.assign(t.begin(), t.end());
        Image top;
        CHECK(loadHex(raw, top, err, 0xFFFFFF) && top.bytes.count(0xFFFFFF) == 1,
              ("IEEE: data up to FFFFFF loads: " + err).c_str());
        t = ":020000021000EC\n:0100000055AA\n:00000001FF\n";
        raw.assign(t.begin(), t.end());
        Image alt;
        CHECK(loadHex(raw, alt, err) && alt.bytes.count(0x10000) == 1,
              ("default limit: a type 02 base past 64K loads as before: " + err).c_str());
    }

    {
        // `bus` in a [machine] table written AFTER the boards: refused, on its own line.
        Machine m;
        std::string err;
        bool ok = loadTomlText("[[board]]\ntype = \"memory\"\nid = \"mem0\"\n"
                               "[machine]\nname = \"t\"\nbus = \"ieee696\"\n",
                               "t.toml", m, err);
        CHECK(!ok && err.find("line 6") != std::string::npos &&
                  err.find("pascal100 has one bus") != std::string::npos,
              ("bus after the boards names its own line: " + err).c_str());
    }

    {
        // Any case of the key; ext takes 1-2 hex digits only.
        Machine m;
        auto* mb = ram64(m);
        std::string err;
        CHECK(setProperty(*mb, "EXT", "true", err), ("SET EXT, any case: " + err).c_str());
        CHECK(setProperty(*mb, "Sixteen_Bit", "on", err), ("SET Sixteen_Bit, any case: " + err).c_str());
        Machine i;
        auto* ib = ram64(i);
        for (const char* v : {"01", "any", "a", "FF"})
            CHECK(!setProperty(*ib, "ext", v, err), (std::string("ext '") + v + "' is refused").c_str());
        CHECK(setProperty(*ib, "ext", "on", err) && setProperty(*ib, "ext", "False", err),
              "ext is a bool, any case");
    }

    {
        // A MEM breakpoint is a physical address: 6 digits, also below 64K.
        Machine m;
        m.debug.add(BreakKind::MemWrite, 0x1234, 0x1234, nullptr, BreakAction::TraceOn);
        CHECK(m.debug.breakpoints().at(0).describe().rfind("mem w  001234 ", 0) == 0,
              ("6 digits: " + m.debug.breakpoints().at(0).describe()).c_str());
    }

    {
        // Two boards that fight in segment 01 only: SHOW BUS CONTENTION and MCP
        // bus_contention walk that segment (Machine::contentionSegments).
        Machine m;
        std::string err;
        Monitor mon(m);
        std::ostringstream o;
        for (const char* id : {"mem1", "mem2"}) {
            mon.exec(std::string("BOARDS ADD memory ") + id, o);
            mon.exec(std::string("SET ") + id + " ext=true", o);
            mon.exec(std::string("REGION ADD ") + id + " type=ram at=10000 size=256", o);
        }
        o.str("");
        mon.exec("SHOW BUS CONTENTION", o);
        CHECK(o.str().find("010000 read  driven by mem1 mem2") != std::string::npos,
              ("SHOW BUS CONTENTION walks segment 01: " + o.str()).c_str());
        std::istringstream in(
            R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})" "\n"
            R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"bus_contention","arguments":{}}})" "\n");
        std::ostringstream out;
        runMcp(m, in, out);
        CHECK(out.str().find("\"addr\":65536") != std::string::npos,
              "MCP bus_contention walks segment 01");
    }

    SECTION("every board type");

    {
        // Each registry board goes in, its I/O decode depends on A0-A7 only (board.h
        // contract) and its memory decode ignores A16-A23 (no board decodes the extended
        // address by default).
        int types = 0;
        for (const BoardType& bt : boardTypes()) {
            ++types;
            {
                Machine m;
                std::string err;
                Board* b = m.add(bt.name, "b0", err);
                CHECK(b != nullptr, (bt.name + " goes in: " + err).c_str());
                if (!b) continue;
                b->power();

                bool ioSame = true, memSame = true;
                for (int p = 0; p < 256 && ioSame; ++p)
                    for (Cycle cy : {Cycle::IoRead, Cycle::IoWrite})
                        for (uint16_t hi : {0x0000, 0xA500, 0xFF00, (int)(p << 8)}) {
                            BusCycle lo{cy, (uint16_t)p, 0, false};
                            BusCycle wide{cy, (uint16_t)(hi | p), 0, false};
                            if (b->decodes(lo) != b->decodes(wide)) ioSame = false;
                        }
                for (int pg = 0; pg < 256 && memSame; ++pg)
                    for (Cycle cy : {Cycle::MemRead, Cycle::MemWrite}) {
                        BusCycle s0{cy, (uint16_t)(pg << 8), 0, false};
                        BusCycle s5{cy, (uint16_t)(pg << 8), 0, false};
                        s5.ext = 0x5A;
                        if (b->decodes(s0) != b->decodes(s5)) memSame = false;
                    }
                CHECK(ioSame, (bt.name + ": I/O decode by A0-A7 only").c_str());
                CHECK(memSame, (bt.name + ": memory decode ignores A16-A23").c_str());
            }
        }
        CHECK(types > 15, "the whole registry was walked");
    }
}
