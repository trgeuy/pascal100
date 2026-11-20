// The CompuPro CPU 286, board 451B (src/boards/compupro-cpu286.cpp): the 80286's reset
// through jumpers J3-J6, the memory manager at 01F8-01FE, the card's own ports and J2, the
// EPROM sockets in page FF (J7), the J1 wait states, the snapshot -- and the boot through the
// Disk 1's EPROM at 0FFFF0, as shipped.
#include "test.h"

#include "boards/compupro-cpu286.h"
#include "boards/s100-memory.h"
#include "core/debug.h"
#include "core/machine.h"
#include "core/statefile.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
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
        CHECK(clocks("A", wr) == clocks("", wr) + 4, "A: one on each of the 4 memory cycles "
                                                    "(3 fetch bytes + the write)");
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
}
