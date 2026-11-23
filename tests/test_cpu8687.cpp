// The CompuPro CPU 86/87 (src/boards/compupro-cpu8687.cpp; reference/CompuPro CPU 86-87.md):
// the 8086 on a 16-bit bus, the card's own ports (the Memory Manager at 00FD and the 80130 at
// FFF0-FFFF, which decode A8-A15 and so are claimed by the card, not decoded on the bus),
// the 80130's hard-wired timer modes, the power-on jump, RESET, the I/O wait state, the
// snapshot -- and the 8086 boot through the Disk 1's EPROM at FFFF0.
#include "test.h"

#include "boards/compupro-cpu8687.h"
#include "boards/compupro-disk1.h"
#include "boards/s100-memory.h"
#include "core/debug.h"
#include "core/machine.h"
#include "core/statefile.h"
#include "io16_board.h"

#include <memory>
#include <string>
#include <vector>

using namespace altair;

namespace {

struct Rig {
    Machine m;
    CompuProCpu8687Board* card = nullptr;
    MemoryBoard* mem = nullptr;

    // 2 MB of 16-bit RAM from 000000, so the Memory Manager has somewhere to go.
    Rig(std::initializer_list<std::pair<const char*, const char*>> props = {},
        bool sixteen = true) {
        std::string err;
        m.bus.setVerify(true);
        m.bus.setUnclaimedPolicy(Unclaimed::Halt);   // the card's own ports must not trip it
        mem = dynamic_cast<MemoryBoard*>(m.add("memory", "mem0", err));
        CHECK(setProperty(*mem, "ext", "true", err), ("mem0 ext: " + err).c_str());
        CHECK(setProperty(*mem, "fill", "zero", err), ("mem0 fill: " + err).c_str());
        CHECK(setProperty(*mem, "sixteen_bit", sixteen ? "true" : "false", err),
              ("mem0 sixteen_bit: " + err).c_str());
        Region r;
        r.kind = RegionKind::Ram;
        r.at = 0;
        r.size = 0x200000;
        CHECK(mem->addRegion(r, err), ("mem0 2M: " + err).c_str());
        mem->power();
        card = dynamic_cast<CompuProCpu8687Board*>(m.add("cpu8687", "cpu", err));
        CHECK(card != nullptr, ("the machine takes a cpu8687: " + err).c_str());
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
// Masked before the cast: MSVC warns (C4310) when a cast truncates a constant.
#define OUTDX(port, v) 0xBA, (uint8_t)((port) & 0xFF), (uint8_t)(((port) >> 8) & 0xFF), 0xB0, (v), 0xEE

} // namespace

void test_cpu8687() {
    SECTION("cpu8687: power-on -- an 8086 at FFFF0, the Memory Manager clear");
    {
        Rig g;
        CHECK(g.m.cpu() == g.card->i8086() && std::string(g.card->i8086()->isa()) == "8086",
              "the active core is an 8086");
        CHECK(g.card->i8086()->pc() == 0xFFFF0, "it starts at FFFF0");
        CHECK(g.card->memoryManager() == 0, "the Memory Manager is cleared");
    }

    SECTION("cpu8687: a 16-bit bus -- one cycle to 16-bit RAM");
    {
        Rig g;
        int wide = 0;
        int h = g.m.bus.observe([&wide](const BusCycle& c) {
            if (c.type == Cycle::MemWrite && c.addr == 0x0200 && c.xtrq) ++wide;
        });
        g.load(0xFFFF0, {0xB8, 0x34, 0x12, 0xA3, 0x00, 0x02, 0xF4});  // MOV AX,1234 MOV [200],AX HLT
        RunResult r = g.run();
        g.m.bus.unobserve(h);
        CHECK(r.why == StopReason::Halted, "it halts");
        CHECK(g.peek(0x0200) == 0x34 && g.peek(0x0201) == 0x12 && wide == 1,
              "the word went in ONE 16-bit cycle (sXTRQ*)");
    }

    SECTION("cpu8687: the Memory Manager -- OUT 00FD, D7-D4 = A23-A20, not decoded on the bus");
    {
        Rig g;
        // MOV AL,10  OUT FD,AL  -- the next fetch is at 1FFFF4, where MOV [1234],AL  HLT wait.
        g.load(0xFFFF0, {0xB0, 0x10, 0xE6, 0xFD});
        g.load(0x1FFFF4, {0xA2, 0x34, 0x12, 0xF4});
        RunResult r = g.run();
        CHECK(r.why == StopReason::Halted, "no unclaimed-port halt: the card owns 00FD");
        CHECK(g.card->memoryManager() == 0x10, "the OUT latched 10");
        CHECK(g.peek(0x101234) == 0x10 && g.peek(0x001234) == 0x00,
              "the fetch and the store went to 1xxxxx");
    }

    SECTION("cpu8687: mm_port -- A15-A8 from S4, the low byte wired to FD");
    {
        Rig g({{"mm_port", "12FD"}});
        // OUT FD,AL (port 00FD) is not the Memory Manager now; OUT DX=12FD is.
        g.load(0xFFFF0, {0xB0, 0x10, 0xE6, 0xFD, 0xF4});
        RunResult r = g.run();
        CHECK(r.why == StopReason::Unclaimed && g.card->memoryManager() == 0,
              "00FD is an ordinary bus port: nothing decodes it, and the latch is untouched");
        Rig k({{"mm_port", "12FD"}});
        k.load(0xFFFF0, {OUTDX(0x12FD, 0x20)});
        k.load(0x2FFFF6, {0xF4});
        k.run();
        CHECK(k.card->memoryManager() == 0x20, "OUT DX=12FD latched 20");
        std::string err;
        CHECK(!setProperty(*k.card, "mm_port", "0100", err) && err.find("FD") != std::string::npos,
              "a port whose low byte is not FD is refused");
    }

    SECTION("cpu8687: the 80130's interrupt controller -- ICW1 1B, ICW2, ICW4, the mask");
    {
        Rig g;
        // The manual's minimum: ICW1 = 1B to FFF0, ICW2 = 80, ICW4 = 0D to FFF2; then the
        // mask A5, read back from FFF2 into [0200].
        g.load(0xFFFF0, {0xEA, 0x00, 0x01, 0x00, 0x00});   // JMPF 0000:0100
        g.load(0x00100, {OUTDX(0xFFF0, 0x1B), OUTDX(0xFFF2, 0x80), 0xB0, 0x0D, 0xEE,
                         0xB0, 0xA5, 0xEE, 0xB0, 0x00, 0xEC, 0xA2, 0x00, 0x02, 0xF4});
        RunResult r = g.run();
        CHECK(r.why == StopReason::Halted, "no unclaimed-port halt: the card owns FFF0/FFF2");
        CHECK(g.card->pic().initialized() && g.card->pic().imr() == 0xA5,
              "the 80130 took the ICWs and the mask");
        CHECK(g.peek(0x0200) == 0xA5, "IN FFF2 read the mask back");
    }

    SECTION("cpu8687: io_80130 off -- its ports go to the bus, and float");
    {
        Rig g({{"io_80130", "false"}});
        g.m.bus.setUnclaimedPolicy(Unclaimed::Silent);
        g.load(0xFFFF0, {0xBA, 0xF2, 0xFF, 0xEC, 0xA2, 0x00, 0x02, 0xF4});  // IN AL,DX=FFF2
        g.run();
        CHECK(g.peek(0x0200) == 0xFF, "IN FFF2 read the floating bus");
    }

    SECTION("cpu8687: the 80130's timers -- hard-wired modes, the latch command");
    {
        Rig g;
        // 3A to FFFE (the manual's byte for timer 0: mode 5 in its mode bits), count 1000.
        g.load(0xFFFF0, {0xEA, 0x00, 0x01, 0x00, 0x00});
        g.load(0x00100, {OUTDX(0xFFFE, 0x3A), OUTDX(0xFFF8, 0x00), 0xB0, 0x10, 0xEE,
                         // latch timer 0, read LSB and MSB into [0200]/[0201]
                         OUTDX(0xFFFE, 0x00), 0xBA, 0xF8, 0xFF, 0xEC, 0xA2, 0x00, 0x02,
                         0xEC, 0xA2, 0x01, 0x02, 0xF4});
        g.run();
        const std::string d = g.card->describe80130();
        CHECK(d.find("C0 mode2") != std::string::npos,
              ("timer 0 is in mode 2 whatever the byte said: " + d).c_str());
        const unsigned count = g.peek(0x0200) | (g.peek(0x0201) << 8);
        CHECK(count > 0 && count < 0x1000, "the latched count is below the 1000 loaded");
    }

    SECTION("cpu8687: power-on jump -- JMPF ss00:0000, at RESET only with S5-9");
    {
        Rig g({{"power_on_jump", "true"}, {"poj_addr", "0D000"}});
        uint16_t cs = 0, ip = 0;
        g.card->i8086()->pcSegOff(cs, ip);
        CHECK(g.card->i8086()->pc() == 0x0D000 && cs == 0x0D00 && ip == 0,
              "power-on: CS:IP = 0D00:0000");
        g.card->reset(Reset::Bus);
        CHECK(g.card->i8086()->pc() == 0xFFFF0, "RESET without jump_on_reset: FFFF0");
        Rig k({{"power_on_jump", "true"}, {"poj_addr", "00000"}, {"jump_on_reset", "true"}});
        k.card->reset(Reset::Bus);
        k.card->i8086()->pcSegOff(cs, ip);
        CHECK(cs == 0x0000 && ip == 0x0000, "RESET with jump_on_reset: 0000:0000, not FFFF:0010");
        std::string err;
        CHECK(!setProperty(*k.card, "poj_addr", "0D001", err), "poj_addr is a 4K boundary");
    }

    SECTION("cpu8687: RESET clears the Memory Manager");
    {
        Rig g;
        g.load(0xFFFF0, {0xB0, 0x10, 0xE6, 0xFD});
        g.load(0x1FFFF4, {0xF4});
        g.run();
        g.card->reset(Reset::Bus);
        CHECK(g.card->memoryManager() == 0, "CLEAR* cleared U46");
    }

    SECTION("cpu8687: io_wait -- one wait state per I/O cycle");
    {
        Rig on, off({{"io_wait", "false"}});
        for (Rig* g : {&on, &off}) {
            g->m.bus.setUnclaimedPolicy(Unclaimed::Silent);
            g->load(0xFFFF0, {0xE6, 0x10, 0xF4});   // OUT 10,AL  HLT
        }
        RunResult a = on.run(1), b = off.run(1);
        CHECK(a.tStates == b.tStates + 1, "the OUT took one clock more with io_wait on");
    }

    SECTION("cpu8687: the snapshot -- the 8086, the Memory Manager, the 80130");
    {
        Rig g;
        g.load(0xFFFF0, {0xEA, 0x00, 0x01, 0x00, 0x00});
        g.load(0x00100, {OUTDX(0xFFF0, 0x1B), OUTDX(0xFFF2, 0x80), 0xB0, 0x0D, 0xEE,
                         0xB0, 0x3C, 0xEE, 0xB0, 0x10, 0xE6, 0xFD});
        g.load(0x10010C, {0xF4});
        g.run();
        StateWriter w;
        g.card->serialize(w);
        Rig k;
        StateReader r(w.data());
        k.card->deserialize(r);
        CHECK(r.ok() && r.remaining() == 0, "the snapshot reads back to the last byte");
        CHECK(k.card->memoryManager() == 0x10 && k.card->pic().imr() == 0x3C &&
                  k.card->i8086()->pc() == g.card->i8086()->pc(),
              "the latch, the 80130's mask and the 8086's PC travel");
    }

    SECTION("cpu8687: the 8086 boots through the Disk 1's EPROM at FFFF0 (routine 0)");
    {
        // The Disk 1 answers every memory read while it boots (its PROM ENA*), so the 8086
        // reads byte F0 of routine 0 at FFFF0 -- JMPF 0000:0006 -- and goes on in the same
        // 256-byte page at 0000.
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
        auto* cpu = dynamic_cast<CompuProCpu8687Board*>(m.add("cpu8687", "cpu", err));
        m.power();
        m.debug.run(1);
        uint16_t cs = 0, ip = 0;
        cpu->i8086()->pcSegOff(cs, ip);
        CHECK(cs == 0x0000 && ip == 0x0006, "the first instruction was JMPF 0000:0006");
        m.debug.run(1);
        cpu->i8086()->pcSegOff(cs, ip);
        CHECK(cs == 0x0000 && ip == 0x0008, "...and the next came from the ROM at 0006 (XOR AX,AX)");
    }

    SECTION("cpu8687: flat out, the clock divides by 8 MHz (the 8086 as shipped)");
    {
        Rig g;
        CHECK(g.m.clock.free() && g.m.clock.hz() == 8000000, "clock_hz 0: flat out at 8 MHz");
    }

    SECTION("cpu8687: 16-bit I/O -- a word at an even port is one cycle to a SIXTN* board");
    {
        // The I/O cycles the run made: type, port, sXTRQ*.
        struct IoSeen { Cycle type; uint16_t addr; bool xtrq; };
        auto watch = [](Rig& g, std::vector<IoSeen>& v) {
            return g.m.bus.observe([&v](const BusCycle& c) {
                if (c.type == Cycle::IoRead || c.type == Cycle::IoWrite)
                    v.push_back({c.type, c.addr, c.xtrq});
            });
        };

        // MOV AX,BEEF  OUT 70,AX  HLT
        Rig g;
        auto* io = new Io16Board;
        io->id = "io16";
        g.m.adopt(std::unique_ptr<Board>(io));
        std::vector<IoSeen> s;
        int h = watch(g, s);
        g.load(0xFFFF0, {0xB8, 0xEF, 0xBE, 0xE7, 0x70, 0xF4});
        RunResult r = g.run();
        g.m.bus.unobserve(h);
        CHECK(r.why == StopReason::Halted && io->reg[0] == 0xEF && io->reg[1] == 0xBE,
              "OUT 70,AX: EF to port 70, BE to port 71");
        CHECK(s.size() == 1 && s[0].type == Cycle::IoWrite && s[0].addr == 0x0070 && s[0].xtrq &&
                  io->writes16 == 1 && io->writes == 0,
              "...in ONE 16-bit I/O cycle (sXTRQ*)");

        // IN AX,70  MOV [0200],AX  HLT
        Rig k;
        auto* io2 = new Io16Board;
        io2->id = "io16";
        k.m.adopt(std::unique_ptr<Board>(io2));
        s.clear();
        h = watch(k, s);
        k.load(0xFFFF0, {0xE5, 0x70, 0xA3, 0x00, 0x02, 0xF4});
        r = k.run();
        k.m.bus.unobserve(h);
        CHECK(r.why == StopReason::Halted && k.peek(0x0200) == 0x11 && k.peek(0x0201) == 0x22,
              "IN AX,70: AL from port 70, AH from port 71");
        CHECK(s.size() == 1 && s[0].type == Cycle::IoRead && s[0].xtrq && io2->reads16 == 1,
              "...in ONE 16-bit I/O cycle");

        // IN AX,71 (an odd port): two byte cycles, 71 then 72, even to a 16-bit board.
        Rig o;
        o.m.bus.setUnclaimedPolicy(Unclaimed::Silent);   // port 72: nobody
        auto* io3 = new Io16Board;
        io3->id = "io16";
        o.m.adopt(std::unique_ptr<Board>(io3));
        s.clear();
        h = watch(o, s);
        o.load(0xFFFF0, {0xE5, 0x71, 0xF4});
        r = o.run();
        o.m.bus.unobserve(h);
        CHECK(s.size() == 2 && s[0].addr == 0x0071 && s[1].addr == 0x0072 && !s[0].xtrq &&
                  !s[1].xtrq && io3->reads16 == 0,
              "IN AX,71: two byte cycles, 71 then 72");
    }

    SECTION("cpu8687: 16-bit I/O -- the byte-serial pair costs 4 clocks and one more I/O wait");
    {
        // MOV AX,BEEF  OUT 70,AX: the OUT's clocks, with a 16-bit and an 8-bit board.
        auto outClocks = [](bool sixteen, bool ioWait) {
            Rig g;
            std::string err;
            CHECK(setProperty(*g.card, "io_wait", ioWait ? "true" : "false", err),
                  ("io_wait: " + err).c_str());
            auto* io = new Io16Board;
            io->id = "io16";
            io->sixteen = sixteen;
            g.m.adopt(std::unique_ptr<Board>(io));
            g.load(0xFFFF0, {0xB8, 0xEF, 0xBE, 0xE7, 0x70, 0xF4});
            g.run(1);
            return g.run(1).tStates;
        };
        const uint64_t w16 = outClocks(true, true), w8 = outClocks(false, true);
        CHECK(w8 == w16 + 5,
              ("io_wait on: byte-serial = 4 clocks + 1 wait more (" + std::to_string(w16) + ", " +
               std::to_string(w8) + ")").c_str());
        CHECK(outClocks(false, false) == outClocks(true, false) + 4,
              "io_wait off: byte-serial = 4 clocks more");
    }

    SECTION("cpu8687: 16-bit I/O -- a breakpoint on the odd port stops the 16-bit IN");
    {
        Rig g;
        auto* io = new Io16Board;
        io->id = "io16";
        g.m.adopt(std::unique_ptr<Board>(io));
        g.load(0xFFFF0, {0xE5, 0x70, 0xF4});   // IN AX,70  HLT
        int bp = g.m.debug.add(BreakKind::IoRead, 0x71, 0x71);
        RunResult r = g.run();
        CHECK(r.why == StopReason::Breakpoint && r.bp == bp && g.card->i8086()->pc() == 0xFFFF0,
              "BREAK IO R 71 stops the one IN16 at 70, before it runs");
    }
}
