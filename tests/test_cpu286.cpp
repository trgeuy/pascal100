// The CompuPro CPU 286, board 451B (src/boards/compupro-cpu286.cpp): the 80286's reset
// through jumpers J3-J6, the memory manager at 01F8-01FE, the card's own ports and J2, the
// EPROM sockets in page FF (J7), the J1 wait states, the snapshot -- and the boot through the
// Disk 1's EPROM at 0FFFF0, as shipped.
#include "test.h"

#include "boards/compupro-cpu286.h"
#include "boards/s100-memory.h"
#include "cli/monitor.h"
#include "core/debug.h"
#include "core/machine.h"
#include "core/statefile.h"
#include "io16_board.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace altair;

namespace {

struct Rig286b {
    Machine m;
    CompuProCpu286Board* card = nullptr;
    MemoryBoard* mem = nullptr;

    // 2 MB of 16-bit RAM from 000000, and 64K at FF0000 (page FF on the S-100 bus).
    explicit Rig286b(std::initializer_list<std::pair<const char*, std::string>> props = {}) {
        std::string err;
        m.bus.setVerify(true);
        m.bus.setUnclaimedPolicy(Unclaimed::Halt);   // the card's own ports must not trip it
        mem = dynamic_cast<MemoryBoard*>(m.add("memory", "mem0", err));
        CHECK(setProperty(*mem, "ext", "true", err), ("mem0 ext: " + err).c_str());
        CHECK(setProperty(*mem, "fill", "zero", err), ("mem0 fill: " + err).c_str());
        CHECK(setProperty(*mem, "sixteen_bit", "true", err), ("mem0 sixteen_bit: " + err).c_str());
        Region r;
        r.kind = RegionKind::Ram;
        r.at = 0;
        r.size = 0x200000;
        CHECK(mem->addRegion(r, err), ("mem0 2M: " + err).c_str());
        Region top;
        top.kind = RegionKind::Ram;
        top.at = 0xFF0000;
        top.size = 0x10000;
        CHECK(mem->addRegion(top, err), ("mem0 page FF: " + err).c_str());
        mem->power();
        card = dynamic_cast<CompuProCpu286Board*>(m.add("cpu286", "cpu", err));
        CHECK(card != nullptr, ("the machine takes a cpu286: " + err).c_str());
        for (auto& [k, v] : props)
            CHECK(setProperty(*card, k, v, err), (std::string(k) + ": " + err).c_str());
        card->power();
    }
    void load(uint32_t at, std::initializer_list<uint8_t> code) {
        for (uint8_t b : code) m.bus.memWriteX(at++, b);
    }
    uint8_t peek(uint32_t a) { return m.bus.peekX(a); }
    RunResult run(uint64_t n = 1000) { return m.debug.run(n); }
};

// MOV DX,port  MOV AL,v  OUT DX,AL
#define OUTDX286(port, v) 0xBA, (uint8_t)((port) & 0xFF), (uint8_t)(((port) >> 8) & 0xFF), 0xB0, (v), 0xEE

// A 16K socket image: `code` at offset 3FF0 (FFFFF0 and every 16K below it), FF elsewhere.
std::string romFile(const char* name, std::initializer_list<uint8_t> code) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::vector<uint8_t> img(0x4000, 0xFF);
    size_t i = 0x3FF0;
    for (uint8_t b : code) img[i++] = b;
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(img.data()), (std::streamsize)img.size());
    return path.string();
}

} // namespace

void test_cpu286() {
    SECTION("cpu286: power-on -- an 80286, the memory manager clear, first fetch at 0FFFF0");
    {
        Rig286b g;
        CHECK(g.m.cpu() == g.card->i80286() && std::string(g.card->i80286()->isa()) == "80286",
              "the active core is an 80286");
        CHECK(g.card->i80286()->pc() == 0xFFFFF0, "the 80286's own PC is FFFFF0");
        CHECK(g.card->memoryManager() == 0, "the memory manager is cleared");
        g.load(0x0FFFF0, {0xF4});                   // HLT at 0FFFF0
        g.load(0xFFFFF0, {0x90, 0x90, 0xF4});       // NOP NOP HLT at FFFFF0
        RunResult r = g.run(5);
        CHECK(r.why == StopReason::Halted && g.card->i80286()->pc() == 0xFFFFF1,
              "J3-J6 A-C (as shipped): A20-A23 = 0, the HLT at 0FFFF0 ran");
    }

    SECTION("cpu286: mm_lines 0 (J3-J6 B-C) -- the 80286's A20-A23, first fetch at FFFFF0");
    {
        Rig286b g({{"mm_lines", "0"}});
        g.load(0x0FFFF0, {0xF4});
        g.load(0xFFFFF0, {0xB8, 0x34, 0x12, 0xF4}); // MOV AX,1234  HLT
        g.run(5);
        CHECK(g.card->i80286()->registers()[9].get() == 0x1234, "MOV AX,1234 ran from FFFFF0");
    }

    SECTION("cpu286: the memory manager -- a byte to an even port 01F8-01FE, D0-D3 = A20-A23");
    {
        Rig286b g;
        // At 0FFFF0: JMPF 0000:0100. At 0100: OUT 01FA,01, then MOV [0200],AL: lands at 100200.
        g.load(0x0FFFF0, {0xEA, 0x00, 0x01, 0x00, 0x00});
        g.load(0x000100, {OUTDX286(0x01FA, 0x01)});
        g.load(0x100106, {0xA2, 0x00, 0x02, 0xF4}); // the fetch after the OUT is at 100106
        RunResult r = g.run(20);
        CHECK(r.why == StopReason::Halted, "it ran on at 100106 and halted");
        CHECK(g.card->memoryManager() == 0x1, "the latch holds 1");
        CHECK(g.peek(0x100200) == 0x01 && g.peek(0x000200) == 0x00, "the write went to 100200");
    }

    SECTION("cpu286: the memory manager -- an odd port, and only D0-D3");
    {
        Rig286b g;
        g.load(0x0FFFF0, {0xEA, 0x00, 0x01, 0x00, 0x00});
        g.load(0x000100, {OUTDX286(0x01FB, 0x03), OUTDX286(0x01FC, 0xF0), 0xF4});
        g.run(20);
        CHECK(g.card->memoryManager() == 0x0, "an odd port leaves it; F0 has no low nibble");
    }

    SECTION("cpu286: the card's own ports -- claimed (not unclaimed), reads float");
    {
        Rig286b g;
        // IN AL,F8 (the absent 80287); IN from 01F8 (the write-only latch); HLT.
        g.load(0x0FFFF0, {0xE4, 0xF8, 0xBA, 0xF8, 0x01, 0xEC, 0xF4});
        RunResult r = g.run(10);
        CHECK(r.why == StopReason::Halted, "neither IN tripped the unclaimed-port halt");
        CHECK((g.card->i80286()->registers()[9].get() & 0xFF) == 0xFF, "the latch reads FF");
    }

    SECTION("cpu286: j2 -- the ports go back to the bus, the memory manager cannot be written");
    {
        Rig286b g({{"j2", "true"}});
        g.m.bus.setUnclaimedPolicy(Unclaimed::Silent);
        g.load(0x0FFFF0, {OUTDX286(0x01F8, 0x01), 0xF4});
        g.run(10);
        CHECK(g.card->memoryManager() == 0x0, "the latch stays 0");
        CHECK(g.card->ioMap().empty(), "SHOW BUS lists no card ports");
    }

    SECTION("cpu286: sockets (J7) -- page FF from the EPROM pair, repeated, writes lost");
    {
        // At FFFFF0 in the ROM: MOV AX,[FFE0] (CS-relative via CS: prefix), MOV [FFE0],AX
        // (back into the ROM: lost), JMPF 0000:0100.
        const std::string rom = romFile("cpu286-test.rom",
            {0x2E, 0xA1, 0xE0, 0xFF, 0x2E, 0xA3, 0xE0, 0xFF, 0xEA, 0x00, 0x01, 0x00, 0x00});
        Rig286b g({{"mm_lines", "0"}, {"sockets", "true"}, {"rom", rom}});
        g.load(0xFFFFF0, {0xF4});                   // the S-100 RAM behind: must not run
        g.load(0x000100, {0xF4});
        RunResult r = g.run(10);
        CHECK(r.why == StopReason::Halted && g.card->i80286()->pc() == 0x101,
              "the ROM ran from FFFFF0 and jumped to 0000:0100");
        CHECK(g.card->i80286()->registers()[9].get() == 0xFFFF,
              "the word at FFFFE0 came from the ROM (FF FF), in one cycle");
        CHECK(g.card->peekCodeByte(g.card->i80286(), g.m.bus, 0xFFBFF0) == 0x2E,
              "a 16K pair repeats through page FF (FFBFF0 = FFFFF0)");
        CHECK(g.peek(0xFFFFE0) == 0x00, "the S-100 RAM at FFFFE0 never saw the write");
        std::filesystem::remove(rom);
    }

    SECTION("cpu286: sockets off (as shipped) -- page FF is S-100 memory");
    {
        const std::string rom = romFile("cpu286-test2.rom", {0xF4});
        Rig286b g({{"mm_lines", "0"}, {"rom", rom}});
        g.load(0xFFFFF0, {0x90, 0xF4});
        g.run(5);
        CHECK(g.card->i80286()->pc() == 0xFFFFF2, "NOP HLT ran from the S-100 RAM");
        std::filesystem::remove(rom);
    }

    SECTION("cpu286: waits (J1) -- additive, by the kind of cycle");
    {
        auto clocks = [](const char* w, std::initializer_list<uint8_t> code) {
            Rig286b g({{"waits", w}});
            g.m.bus.setUnclaimedPolicy(Unclaimed::Silent);
            g.load(0x0FFFF0, code);
            return g.run(1).tStates;
        };
        const auto out = std::initializer_list<uint8_t>{0xE6, 0x10};          // OUT 10,AL
        const auto wr = std::initializer_list<uint8_t>{0xA2, 0x00, 0x02};     // MOV [0200],AL
        CHECK(clocks("B", out) == clocks("", out) + 1, "B: one wait on the I/O cycle");
        CHECK(clocks("BE", out) == clocks("", out) + 3, "B + E: three");
        CHECK(clocks("H", wr) == clocks("", wr) + 1, "H: one on the memory write");
        CHECK(clocks("A", wr) == clocks("", wr) + 3, "A: one on each of the 3 memory cycles "
                                                    "(2 fetch words + the write)");
    }

    SECTION("cpu286: code is fetched a word at a time -- two NOPs in one word, one cycle");
    {
        auto clocks = [](const char* w, int steps) {
            Rig286b g({{"waits", w}});
            g.load(0x0FFFF0, {0x90, 0x90, 0x90, 0xF4});
            return g.run((uint64_t)steps).tStates;
        };
        CHECK(clocks("A", 1) == clocks("", 1) + 1, "the first NOP: one word fetch");
        CHECK(clocks("A", 2) == clocks("", 2) + 1, "the second NOP is in that word: no cycle");
        CHECK(clocks("A", 3) == clocks("", 3) + 2, "the third NOP: the next word");
    }

    SECTION("cpu286: DISASM after reset reads FFFFF0 (CS base FF0000), not 0FFFF0");
    {
        Rig286b g({{"mm_lines", "0"}});
        g.load(0xFFFFF0, {0xF4});                      // HLT where the 80286 fetches
        g.load(0x0FFFF0, {0xFA});                      // CLI at CS*16+IP
        Monitor mon(g.m);
        std::ostringstream o;
        mon.exec("DISASM", o);
        CHECK(o.str().find("HLT") != std::string::npos && o.str().find("CLI") == std::string::npos,
              ("DISASM decodes the bytes the 80286 fetches: " + o.str()).c_str());
    }

    SECTION("cpu286: RESET clears the memory manager");
    {
        Rig286b g;
        // The fetch after the OUT is at 1FFFF6 (A20 from the latch): HLT there.
        g.load(0x0FFFF0, {OUTDX286(0x01F8, 0x01)});
        g.load(0x1FFFF6, {0xF4});
        RunResult r = g.run(10);
        CHECK(r.why == StopReason::Halted && g.card->memoryManager() == 0x1, "the latch holds 1");
        g.card->reset(Reset::Bus);
        CHECK(g.card->memoryManager() == 0 && g.card->i80286()->pc() == 0xFFFFF0, "cleared");
    }

    SECTION("cpu286: the snapshot -- the 80286 and the memory manager");
    {
        Rig286b g;
        g.load(0x0FFFF0, {OUTDX286(0x01F8, 0x01)});
        g.load(0x1FFFF6, {0xF4});
        g.run(10);
        StateWriter w;
        g.card->serialize(w);
        Rig286b k;
        StateReader r(w.data());
        k.card->deserialize(r);
        CHECK(r.ok() && r.remaining() == 0, "the snapshot reads back to the last byte");
        CHECK(k.card->memoryManager() == 0x1 && k.card->i80286()->pc() == g.card->i80286()->pc() &&
                  k.card->i80286()->csBase() == 0xFF0000,
              "the latch, the PC and the reset CS base travel");
    }

    SECTION("cpu286: the 80286 boots through the Disk 1's EPROM at 0FFFF0 (routine 0)");
    {
        // As shipped (J3-J6 A-C) the first fetch is at 0FFFF0, where the Disk 1 answers every
        // memory read while it boots (p. 12): byte F0 of routine 0 = JMPF 0000:0006.
        Machine m;
        std::string err;
        auto* mem = dynamic_cast<MemoryBoard*>(m.add("memory", "mem0", err));
        Region rg;
        rg.kind = RegionKind::Ram;
        rg.at = 0;
        rg.size = 0x100000;
        mem->addRegion(rg, err);
        setProperty(*mem, "honors_phantom", "read", err);
        auto* fd = m.add("disk1", "fd0", err);
        CHECK(fd && setProperty(*fd, "routine", "0", err), ("routine 0: " + err).c_str());
        auto* cpu = dynamic_cast<CompuProCpu286Board*>(m.add("cpu286", "cpu", err));
        m.power();
        m.debug.run(1);
        uint16_t cs = 0, ip = 0;
        cpu->i80286()->pcSegOff(cs, ip);
        CHECK(cs == 0x0000 && ip == 0x0006, "the first instruction was JMPF 0000:0006");
        m.debug.run(1);
        cpu->i80286()->pcSegOff(cs, ip);
        CHECK(cs == 0x0000 && ip == 0x0008, "...and the next came from the ROM at 0006 (XOR AX,AX)");
    }

    SECTION("cpu286: the 80286 boots through the Disk 1A's EPROM at 0FFFF0 (routine 5, Disk 3)");
    {
        // The Disk 1A also answers every memory read while it boots (manual p. 18), A0-A8 into
        // a 512-byte routine: offset 1F0 of routine 5 = JMPF 0000:0002, then XOR AX,AX at 0002.
        Machine m;
        std::string err;
        auto* mem = dynamic_cast<MemoryBoard*>(m.add("memory", "mem0", err));
        Region rg;
        rg.kind = RegionKind::Ram;
        rg.at = 0;
        rg.size = 0x100000;
        mem->addRegion(rg, err);
        setProperty(*mem, "honors_phantom", "read", err);
        auto* fd = m.add("disk1a", "fd0", err);
        CHECK(fd && setProperty(*fd, "routine", "5", err), ("routine 5: " + err).c_str());
        auto* cpu = dynamic_cast<CompuProCpu286Board*>(m.add("cpu286", "cpu", err));
        m.power();
        CHECK(m.bus.peekX(0x0FFFF0) == 0xEA && m.bus.peekX(0x000002) == 0x33,
              "the ROM shows at 0FFFF0 and at 0000 alike");
        m.debug.run(1);
        uint16_t cs = 0, ip = 0;
        cpu->i80286()->pcSegOff(cs, ip);
        CHECK(cs == 0x0000 && ip == 0x0002, "the first instruction was JMPF 0000:0002");
        m.debug.run(1);
        cpu->i80286()->pcSegOff(cs, ip);
        CHECK(cs == 0x0000 && ip == 0x0004, "...and the next came from the ROM at 0002 (XOR AX,AX)");
    }

    SECTION("cpu286: flat out, the clock divides by 8 MHz (the slowest crystal sold); clock_hz paces");
    {
        Rig286b g;
        CHECK(g.m.clock.free() && g.m.clock.hz() == 8000000, "clock_hz 0: flat out at 8 MHz");
        std::string err;
        CHECK(setProperty(*g.card, "clock_hz", "12500000", err), err.c_str());
        CHECK(!g.m.clock.free() && g.m.clock.hz() == 12500000, "clock_hz 12500000: paced at it");
    }

    SECTION("cpu286: BOARDS REMOVE of the CPU board puts the clock back to 2 MHz, flat out");
    {
        Rig286b g;
        std::string err;
        CHECK(g.m.clock.hz() == 8000000, "flat out at 8 MHz with the board");
        CHECK(g.m.remove("cpu", err), err.c_str());
        CHECK(g.m.clock.free() && g.m.clock.hz() == 2000000,
              "no CPU board: the 88-CPU's 2 MHz, flat out, for the boards left");
    }

    SECTION("cpu286: BREAK IO on a 16-bit port -- FC3E stops on OUT DX to FC3E, 3E still matches the low byte");
    {
        // The 80286 puts all 16 bits of DX on A0-A15 for OUT DX. A breakpoint above FF names
        // a 16-bit port and compares all 16 bits; one at FF or below compares the low byte,
        // as for an 8080, whose OUT puts the port on both halves of the address.
        auto stopsAt = [](uint32_t lo) {
            Rig286b g;
            g.m.bus.setUnclaimedPolicy(Unclaimed::Silent);
            g.load(0x0FFFF0, {OUTDX286(0xFC3E, 0x40), 0xF4});
            int bp = g.m.debug.add(BreakKind::IoWrite, lo, lo);
            RunResult r = g.run(10);
            return r.why == StopReason::Breakpoint && r.bp == bp;
        };
        CHECK(stopsAt(0xFC3E), "BREAK IO W FC3E stops on OUT DX,AL with DX = FC3E");
        CHECK(stopsAt(0x3E), "BREAK IO W 3E still stops on it (the low byte)");
        CHECK(!stopsAt(0xFD3E), "BREAK IO W FD3E does not: the high byte differs");
        Rig286b g;
        int bp = g.m.debug.add(BreakKind::IoWrite, 0xFC3E, 0xFC3E);
        bool listed = false;
        for (const auto& b : g.m.debug.breakpoints())
            if (b.id == bp) listed = b.describe().find("FC3E") != std::string::npos;
        CHECK(listed, "BREAK lists it with all four digits");
    }

    SECTION("cpu286: 16-bit I/O -- one cycle to a SIXTN* board; the card's own ports stay local");
    {
        struct IoSeen { Cycle type; uint16_t addr; bool xtrq; };
        std::vector<IoSeen> s;
        auto watch = [&s](Rig286b& g) {
            return g.m.bus.observe([&s](const BusCycle& c) {
                if (c.type == Cycle::IoRead || c.type == Cycle::IoWrite)
                    s.push_back({c.type, c.addr, c.xtrq});
            });
        };

        // MOV AX,BEEF  OUT 70,AX  IN AX,70  MOV [0200],AX  HLT -- at 0FFFF0, 14 bytes.
        Rig286b g;
        auto* io = new Io16Board;
        io->id = "io16";
        g.m.adopt(std::unique_ptr<Board>(io));
        int h = watch(g);
        g.load(0x0FFFF0, {0xB8, 0xEF, 0xBE, 0xE7, 0x70, 0xE5, 0x70, 0xA3, 0x00, 0x02, 0xF4});
        RunResult r = g.run(10);
        g.m.bus.unobserve(h);
        CHECK(r.why == StopReason::Halted && io->reg[0] == 0xEF && io->reg[1] == 0xBE &&
                  g.peek(0x0200) == 0xEF && g.peek(0x0201) == 0xBE,
              "OUT 70,AX then IN AX,70: the word went out and came back");
        CHECK(s.size() == 2 && s[0].type == Cycle::IoWrite && s[0].xtrq &&
                  s[1].type == Cycle::IoRead && s[1].xtrq && io->writes16 == 1 &&
                  io->reads16 == 1 && io->reads == 0 && io->writes == 0,
              "each in ONE 16-bit I/O cycle (sXTRQ*)");

        // MOV DX,01F8  OUT DX,AX  HLT: the memory manager is the card's own port, so the word
        // is two byte cycles (the S-100 cycles still run), never a 16-bit request.
        Rig286b k;
        s.clear();
        h = watch(k);
        k.load(0x0FFFF0, {0xBA, 0xF8, 0x01, 0xEF, 0xF4});
        r = k.run(10);
        k.m.bus.unobserve(h);
        CHECK(r.why == StopReason::Halted && s.size() == 2 && s[0].addr == 0x01F8 &&
                  s[1].addr == 0x01F9 && !s[0].xtrq && !s[1].xtrq,
              "OUT DX,AX to 01F8 (own port): two byte cycles, no sXTRQ*, no unclaimed halt");
    }
}
