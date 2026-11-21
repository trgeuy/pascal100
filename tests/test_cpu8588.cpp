// The CompuPro CPU 8085/88 (src/boards/compupro-cpu8588.cpp; reference/compupro/
// cpu-8085-88.md): the swap port, the Memory Manager, power-on-jump, reset-on-swap, the IOW
// wait state -- and the debugger on an 8088 that a card holds (a PC breakpoint on a linear
// address, which is what the 32-bit debugger PC is for).
#include "test.h"

#include "boards/compupro-cpu8588.h"
#include "boards/s100-memory.h"
#include "cli/monitor.h"
#include "core/debug.h"
#include "core/machine.h"

#include <sstream>
#include <string>

using namespace altair;

namespace {

struct Rig {
    Machine m;
    Cpu8588Board* card = nullptr;

    // 2 MB of RAM from 000000, so the Memory Manager has somewhere to go.
    Rig(std::initializer_list<std::pair<const char*, const char*>> props = {}) {
        std::string err;
        m.bus.setVerify(true);
        auto* mem = dynamic_cast<MemoryBoard*>(m.add("memory", "mem0", err));
        CHECK(setProperty(*mem, "ext", "true", err), ("mem0 ext: " + err).c_str());
        CHECK(setProperty(*mem, "fill", "zero", err), ("mem0 fill: " + err).c_str());
        Region r;
        r.kind = RegionKind::Ram;
        r.at = 0;
        r.size = 0x200000;
        CHECK(mem->addRegion(r, err), ("mem0 2M: " + err).c_str());
        mem->power();
        card = dynamic_cast<Cpu8588Board*>(m.add("cpu8588", "cpu", err));
        CHECK(card != nullptr, ("the machine takes a cpu8588: " + err).c_str());
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

// The 8088 at its reset address: MOV AL,55  MOV [0100],AL  IN AL,FD  MOV AL,66
// MOV [0101],AL  IN AL,FD. Each IN hands the bus back to the 8085.
void load8088(Rig& g, uint32_t at) {
    g.load(at, {0xB0, 0x55, 0xA2, 0x00, 0x01, 0xE4, 0xFD,
                0xB0, 0x66, 0xA2, 0x01, 0x01, 0xE4, 0xFD});
}

} // namespace

void test_cpu8588() {
    SECTION("cpu8588: power-on -- the 8085 on line at 0000, the 8088 waiting at FFFF0");
    {
        Rig g;
        CHECK(!g.card->on8088() && g.m.cpu() == g.card->i8085(), "the 8085 has the bus");
        CHECK(g.card->i8085()->pc() == 0x0000, "the 8085 starts at 0000");
        CHECK(g.card->i8088()->pc() == 0xFFFF0, "the 8088 is at FFFF0");
        CHECK(g.card->latch() == 0, "the Memory Manager is cleared");
    }

    SECTION("cpu8588: IN FD swaps, each processor resumes where it stopped");
    {
        Rig g;
        g.load(0x0000, {0xDB, 0xFD, 0xDB, 0xFD, 0x76});   // IN FD  IN FD  HLT
        load8088(g, 0xFFFF0);
        RunResult r = g.run();
        CHECK(r.why == StopReason::Halted, "the 8085 halts at the end");
        CHECK(g.peek(0x0100) == 0x55 && g.peek(0x0101) == 0x66,
              "the 8088 ran its first part, then resumed for its second");
        CHECK(!g.card->on8088() && g.card->i8085()->pc() == 0x0005,
              "the 8085 resumed after each IN and halted at 0004");
        uint32_t a = 0;
        for (auto& d : g.card->i8085()->registers())
            if (d.name == "A") a = d.get();
        CHECK(a == 0xFF, "the IN read FF (p.12)");
    }

    SECTION("cpu8588: 8RS -- the 8088 is reset each time it comes on line");
    {
        Rig g({{"swap_reset_8088", "true"}});
        g.load(0x0000, {0xDB, 0xFD, 0xDB, 0xFD, 0x76});
        load8088(g, 0xFFFF0);
        g.run();
        CHECK(g.peek(0x0100) == 0x55 && g.peek(0x0101) == 0x00,
              "the second time it started again at FFFF0, so 66 was never stored");
    }

    SECTION("cpu8588: the Memory Manager -- A16-A23 for the 8085");
    {
        Rig g;
        // MVI A,12  OUT FD  ... the next fetch is in segment 12, where STA 1234 / HLT wait.
        g.load(0x0000, {0x3E, 0x12, 0xD3, 0xFD});
        g.load(0x120004, {0x3E, 0x77, 0x32, 0x34, 0x12, 0x76});   // MVI A,77  STA 1234  HLT
        g.run();
        CHECK(g.card->latch() == 0x12, "the OUT latched 12");
        CHECK(g.peek(0x121234) == 0x77 && g.peek(0x001234) == 0x00,
              "the 8085's store went to 121234");
    }

    SECTION("cpu8588: the Memory Manager -- only A20-A23 for the 8088");
    {
        Rig g;
        g.m.bus.ioWrite(0x00FD, 0x1F);            // the monitor's OUT FD,1F
        g.load(0x1F0000, {0xDB, 0xFD, 0x76});     // the 8085, in segment 1F: IN FD  HLT
        load8088(g, 0x1FFFF0);                    // A20 = 1 from the latch's high nibble
        g.load(0x0FFFF0, {0x90});                 // a NOP where a missing A20 would look
        // The debugger's view (CpuCard::peekCodeByte) adds the same latch bits as the fetch.
        CHECK(g.card->peekCodeByte(g.card->activeCore(), g.m.bus, 0x0000) == 0xDB,
              "debugger peek, 8085: 0000 reads 1F0000 (the whole latch is A16-A23)");
        g.run(1);                                 // the IN: the 8088 comes on line
        CHECK(g.card->peekCodeByte(g.card->activeCore(), g.m.bus, 0xFFFF0) == 0xB0,
              "debugger peek, 8088: FFFF0 reads 1FFFF0 (A20 from the latch), not 0FFFF0");
        g.run(2);                                 // the 8088's two MOVs
        CHECK(g.card->on8088(), "the 8088 is on line");
        CHECK(g.peek(0x100100) == 0x55, "its store went to 100100: A20 from the latch");
        CHECK(g.peek(0x1F0100) == 0x00, "not to 1F0100: the latch's low nibble is not used");
    }

    SECTION("cpu8588: power-on-jump -- C3 00 <S2> on the 8085's first reads");
    {
        Rig g({{"power_on_jump", "true"}, {"poj_addr", "E000"}});
        g.load(0xE000, {0x76});
        RunResult r = g.run();
        CHECK(r.why == StopReason::Halted && g.card->i8085()->pc() == 0xE001,
              "the 8085 jumped to E000 and halted there");
        CHECK(g.peek(0x0000) == 0x00, "memory at 0000 was not changed");
    }

    SECTION("cpu8588: RESET* -- XAC clears the latch, JOR repeats the jump");
    {
        Rig g({{"power_on_jump", "true"}, {"poj_addr", "E000"}});
        g.m.bus.ioWrite(0x00FD, 0x30);
        g.card->reset(Reset::Bus);
        CHECK(g.card->latch() == 0, "xac on: RESET* cleared the latch");
        g.load(0xE000, {0x76});
        g.run();
        CHECK(g.card->i8085()->pc() == 0xE001, "jor on: the jump ran again after RESET*");

        Rig k({{"xac", "false"}});
        k.m.bus.ioWrite(0x00FD, 0x30);
        k.card->reset(Reset::Bus);
        CHECK(k.card->latch() == 0x30, "xac off: the latch survives RESET*");
        k.card->power();
        CHECK(k.card->latch() == 0, "power-on clears it anyway");
    }

    SECTION("cpu8588: IOW -- one wait state per I/O cycle");
    {
        Rig on, off({{"iow", "false"}});
        for (Rig* g : {&on, &off}) g->load(0x0000, {0xD3, 0x10, 0x76});   // OUT 10  HLT
        RunResult a = on.run(1), b = off.run(1);
        CHECK(a.tStates == b.tStates + 1, "the OUT took one T-state more with iow on");
    }

    SECTION("cpu8588: a PC breakpoint on the 8088's linear address");
    {
        Rig g;
        g.load(0x0000, {0xDB, 0xFD, 0x76});
        load8088(g, 0xFFFF0);
        g.m.debug.add(BreakKind::Pc, 0xFFFF2, 0xFFFF2);   // after MOV AL,55
        RunResult r = g.run();
        CHECK(r.why == StopReason::Breakpoint && r.pc == 0xFFFF2,
              "stopped at linear FFFF2 (CS F000 or FFFF, whichever it is)");
        CHECK(g.card->on8088() && g.m.cpu() == g.card->i8088(), "with the 8088 on line");
        CHECK(pcText(*g.m.cpu()) == "FFFF:0002 (FFFF2)", "CS:IP and the linear address");
    }

    SECTION("cpu8588: REGS on the 8088 -- the flags spelled out, three lines");
    {
        Rig g;
        g.load(0x0000, {0xDB, 0xFD});   // IN FD: hand the bus to the 8088
        g.load(0xFFFF0, {0xF4});        // the 8088: HLT
        g.run();
        CHECK(g.card->on8088(), "the 8088 has the bus");
        Monitor            mon(g.m);
        std::ostringstream out;
        mon.exec("REGS", out);
        const std::string s = out.str();
        CHECK(s.find("OF=0 DF=0 IF=0 TF=0 SF=0 ZF=0 AF=0 PF=0 CF=0\n") != std::string::npos,
              "the flags are spelled out, alone on their line (not OF0DF0IF0...)");
        CHECK(s.find("\nAX=") != std::string::npos && s.find("\nCS=") != std::string::npos,
              "the general registers and the segments each start a new line");
    }

    SECTION("cpu8588: flat out, the clock divides by the running processor's crystal: 6 MHz, then 8");
    {
        Rig g;
        CHECK(g.m.clock.free() && g.m.clock.hz() == 6000000, "the 8085 on line: 6 MHz");
        g.load(0x0000, {0xDB, 0xFD, 0x76});   // IN FD (to the 8088)  HLT
        g.load(0xFFFF0, {0xF4});              // the 8088: HLT
        g.run();
        CHECK(g.card->on8088() && g.m.clock.hz() == 8000000, "the 8088 on line: 8 MHz");
    }
}
