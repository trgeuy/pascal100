// CompuPro SPUZ (docs/boards/compupro-spuz.md, DESIGN.md 3.0.4): the window and the ports on
// A0-A15, SLAVESEL and PTLATCH, the reset and the bus hang, the banks, the VI line, the 2681 as
// the board Z80 sees it, and the board's time against the host's.
//
// Every test drives the board as the host does, with real bus cycles at the full 16-bit port
// address. The programs the board Z80 runs are hand-assembled here and loaded through the
// window, after the manual's HLT.CMD sequence (reference/CompuPro SPUZ.md, pp. 28-29).

#include "boards/compupro-spuz.h"
#include "core/bus.h"
#include "core/clock.h"
#include "core/machine.h"
#include "cli/monitor.h"
#include "core/privatecpu.h"
#include "test.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

using namespace altair;

namespace {

constexpr uint16_t SLAVESEL = 0xFC40;
constexpr uint16_t PTLATCH  = 0xFC41;
constexpr uint16_t DUART    = 0xFC30;
constexpr uint32_t WIN      = 0x0E0000;
constexpr uint8_t  ZRST     = 0x08;  // PTLATCH D3: let the Z80 run
constexpr uint64_t MS       = 2000;  // 1 ms of host time at the default 2 MHz

struct Rig {
    Machine    m;
    SpuzBoard* sp = nullptr;

    explicit Rig(int boards = 1) {
        std::string err;
        for (int i = 0; i < boards; ++i) {
            auto* b = dynamic_cast<SpuzBoard*>(m.add("spuz", "spuz" + std::to_string(i), err));
            if (i > 0) setProperty(*b, "select", std::to_string(i), err);
            if (i == 0) sp = b;
        }
        m.bus.setVerify(true);
        m.power();
    }
    SpuzBoard* board(int i) { return dynamic_cast<SpuzBoard*>(m.find("spuz" + std::to_string(i))); }
    void    out(uint16_t port, uint8_t v) { m.bus.ioWrite(port, v); }
    uint8_t in(uint16_t port) { return m.bus.ioRead(port); }
    void    poke(uint32_t a, uint8_t v) { m.bus.memWriteX(WIN | a, v); }
    uint8_t peekWin(uint32_t a) { return m.bus.memReadX(WIN | a); }
    void    run(uint64_t hostT) { m.clock.advance(hostT); }
    bool    z80Halted() { return (in(DUART + 0xD) & 0x40) == 0; }  // IP6 = HALT*

    // HLT.CMD (manual p. 28): select, let the Z80 run, put a HLT at 0, reset, run again.
    // Afterwards the Z80 is halted at 0001h in bank `bank` and the host may load programs.
    void quiesce(uint8_t bank = 3, uint8_t sel = 0) {
        out(SLAVESEL, sel);
        out(PTLATCH, (uint8_t)(ZRST | bank));
        poke(0x0000, 0x76);
        out(PTLATCH, bank);
        out(PTLATCH, (uint8_t)(ZRST | bank));
        run(MS);
    }
    void load(std::initializer_list<uint8_t> code, uint16_t at) {
        uint16_t a = at;
        for (uint8_t b : code) poke(a++, b);
    }
    // Load `code` at 0000h of bank 3 (where the Z80 starts) and start it: assert and release
    // reset (p. 29).
    void start(std::initializer_list<uint8_t> code) {
        quiesce();
        load(code, 0);
        out(PTLATCH, 0x03);
        out(PTLATCH, ZRST | 0x03);
    }
};

// A board Z80 register by name, through the debugger's registers.
uint32_t zreg(SpuzBoard* sp, const char* name) {
    for (RegDef& d : sp->z80().registers())
        if (d.name == name) return d.get();
    return 0xFFFFFFFF;
}

// A host Z80 spinning on JR $ at 0000h, in 4K of RAM, so the run loop has something to run
// while the SPUZ catches up on its 1 ms deadline.
void addSpinningHost(Rig& r) {
    std::string err;
    r.m.add("z80", "cpu0", err);
    Board* mem = r.m.add("memory", "mem", err);
    mem->loadSubUnit("region", {{"type", "ram"}, {"at", "0000"}, {"size", "4K"}}, err);
    r.m.power();
    r.m.bus.memWrite(0x0000, 0x18);  // JR $
    r.m.bus.memWrite(0x0001, 0xFE);
    r.m.cpu()->setPc(0);
}

// The SPUZ loop for the stage 4 tests: LD A,0 / loop: INC A / JR loop (0002h-0004h).
std::initializer_list<uint8_t> kCountLoop = {0x3E, 0x00, 0x3C, 0x18, 0xFD};

std::string mon(Monitor& m, const std::string& line) {
    std::ostringstream o;
    m.exec(line, o);
    return o.str();
}

}  // namespace

void test_spuz() {
    std::string err;

    SECTION("spuz: power-on: the Z80 is in reset, no board is selected, VI0 is pulled");
    {
        Rig r;
        CHECK(r.sp->inReset() && !r.sp->selected(), "in reset and not selected");
        CHECK((r.m.bus.viLines() & 0x01) != 0, "OPR = 0 after reset: OP6 high, Q3 pulls VI0");
        CHECK(r.peekWin(0) == 0xFF && !r.m.bus.takeBusHang(),
              "not selected: the window is not decoded, and nothing hangs");
        CHECK(r.sp->z80Bank() == 3, "the Z80 starts in bank 3");
    }

    SECTION("spuz: the ports decode A8-A15: FC40h selects, 0040h does not");
    {
        Rig r;
        r.out(0x0040, 0x00);
        CHECK(!r.sp->selected(), "OUT 0040h is not SLAVESEL");
        r.out(SLAVESEL, 0x00);
        CHECK(r.sp->selected(), "OUT FC40h, 00h selects board 0");
        r.out(SLAVESEL, 0x01);
        CHECK(!r.sp->selected(), "a select of another board deselects it");
        r.out(SLAVESEL, 0xF0);
        CHECK(r.sp->selected(), "only D3-D0 are compared (F0h selects board 0)");
        CHECK(r.m.bus.takeWaitStates() == 0, "SLAVESEL is not on the internal bus: no wait");
    }

    SECTION("spuz: an access while the Z80 is in reset floats and asks for a BusHang stop");
    {
        Rig r;
        r.out(SLAVESEL, 0x00);
        r.m.drainBoardLog();
        CHECK(r.peekWin(0x1234) == 0xFF, "a window read floats");
        CHECK(r.m.bus.takeBusHang(), "...and asks the run loop to stop");
        r.poke(0x1234, 0x55);
        CHECK(r.m.bus.takeBusHang(), "a window write also hangs");
        CHECK(r.in(DUART + 1) == 0xFF && r.m.bus.takeBusHang(), "IN FC31h hangs");
        auto log = r.m.drainBoardLog();
        CHECK(!log.empty() && log[0].find("in reset") != std::string::npos &&
                  log[0].find("0E1234h") != std::string::npos,
              "the board says why, and names the address");
        r.out(PTLATCH, ZRST);
        r.run(MS);
        CHECK(r.sp->ram(0x31234) == 0x00, "the write in reset went nowhere");
    }

    SECTION("spuz: the run stops with StopReason::BusHang at the instruction boundary");
    {
        Rig r;
        Board* cpu = r.m.add("z80", "cpu0", err);
        CHECK(cpu != nullptr, "a host Z80 goes in");
        Board* mem = r.m.add("memory", "mem", err);
        CHECK(mem && mem->loadSubUnit("region", {{"type", "ram"}, {"at", "0000"}, {"size", "4K"}}, err),
              ("4K of host RAM: " + err).c_str());
        r.m.power();
        // The host Z80 selects the SPUZ with B = FCh on A8-A15, then reads the 2681.
        const uint8_t prog[] = {
            0x01, 0x40, 0xFC,  // LD BC,FC40h
            0xAF,              // XOR A
            0xED, 0x79,        // OUT (C),A    -- SLAVESEL 00h
            0x0E, 0x31,        // LD C,31h
            0xED, 0x78,        // IN A,(C)     -- FC31h, the Z80 in reset
            0x00,              // NOP
            0x76,              // HALT
        };
        for (size_t i = 0; i < sizeof prog; ++i) r.m.bus.memWrite((uint16_t)i, prog[i]);
        r.m.cpu()->setPc(0);
        RunResult rr = r.m.debug.run(100);
        CHECK(rr.why == StopReason::BusHang, "the run stops with BusHang");
        CHECK(r.m.cpu()->pc() == 0x000A, "...after the IN, at the next instruction");
    }

    SECTION("spuz: HLT.CMD: the Z80 runs from 0000h, fetches the HLT and halts");
    {
        Rig r;
        r.quiesce();
        CHECK(!r.sp->inReset() && r.sp->z80().halted(), "the board Z80 is halted");
        CHECK(r.peekWin(0) == 0x76, "CHECK: the 76h is still there");
        CHECK(r.z80Halted(), "IP6 (HALT*) reads low through FC3Dh");
        CHECK(r.sp->ram(0x30000) == 0x76, "the host wrote bank 3 (PTLATCH D1-D0 = 3)");
        r.m.bus.takeWaitStates();
        (void)r.peekWin(0);
        CHECK(r.m.bus.takeWaitStates() == 2, "J1 B: two wait states on a window access");
    }

    SECTION("spuz: PTLATCH D1-D0 pick the host's bank; a 64K board has one");
    {
        Rig r;
        r.quiesce();
        r.out(PTLATCH, ZRST | 0x01);
        r.poke(0x4000, 0x11);
        r.out(PTLATCH, ZRST | 0x02);
        r.poke(0x4000, 0x22);
        CHECK(r.sp->ram(0x14000) == 0x11 && r.sp->ram(0x24000) == 0x22, "banks 1 and 2");
        CHECK((r.in(DUART + 0xD) & 0x30) == 0x20, "IP5/IP4 (SB1/SB0) show the host's bank");

        Rig s;
        setProperty(*s.sp, "memory", "64k", err);
        s.quiesce(0x02);
        s.poke(0x4000, 0x33);
        CHECK(s.sp->ram(0x04000) == 0x33 && s.sp->z80Bank() == 0, "64k: every bank is bank 0");
    }

    SECTION("spuz: OP5/OP4 pick the Z80's bank; the next fetch comes from the new bank");
    {
        Rig r;
        r.quiesce();
        r.out(PTLATCH, ZRST | 0x00);  // the host loads the continuation into bank 0
        r.load({0x3E, 0x55, 0x32, 0x00, 0x10, 0x76}, 0x0004);
        r.out(PTLATCH, 0x03);
        r.out(PTLATCH, ZRST | 0x03);
        // In bank 3: LD A,30h / OUT (0Eh),A -- set OPR bits 5 and 4: OP5/OP4 low, bank 0.
        r.load({0x3E, 0x30, 0xD3, 0x0E}, 0x0000);
        r.out(PTLATCH, 0x03);
        r.out(PTLATCH, ZRST | 0x03);
        r.run(MS);
        CHECK(r.sp->z80Bank() == 0, "the Z80 is in bank 0");
        CHECK(r.sp->ram(0x01000) == 0x55, "the code at 0004h of bank 0 ran");
    }

    SECTION("spuz: OPR bit 6 releases the VI line; resetting it pulls the line again");
    {
        Rig r;
        r.start({0x3E, 0x40, 0xD3, 0x0E,   // LD A,40h / OUT (0Eh),A: set OPR bit 6
                 0x76});
        r.run(MS);
        CHECK((r.m.bus.viLines() & 0x01) == 0, "OP6 low: VI0 released");
        r.start({0x3E, 0x40, 0xD3, 0x0E, 0x3E, 0x40, 0xD3, 0x0F,  // set, then reset OPR 6
                 0x76});
        r.run(MS);
        CHECK((r.m.bus.viLines() & 0x01) != 0, "OUT (0Fh) resets OPR 6: VI0 pulled");
        setProperty(*r.sp, "interrupt", "vi3", err);
        CHECK((r.m.bus.viLines() & 0x09) == 0x08, "J2 on VI3: the line moves");
    }

    SECTION("spuz: the VI line reaches the host by the 1 ms deadline, without a host access");
    {
        Rig r;
        // A delay loop, then release VI0.
        r.start({0x21, 0x00, 0x04,        // LD HL,0400h
                 0x2B, 0x7C, 0xB5,        // DEC HL / LD A,H / OR L
                 0x20, 0xFB,              // JR NZ,-5
                 0x3E, 0x40, 0xD3, 0x0E,  // set OPR bit 6
                 0x76});
        CHECK((r.m.bus.viLines() & 0x01) != 0, "just started: VI0 pulled");
        r.run(20 * MS);  // no host cycle to the board in this time
        CHECK((r.m.bus.viLines() & 0x01) == 0, "the deadline caught the board up");
    }

    SECTION("spuz: the 2681 counter interrupts the Z80 in IM 2, vector at FBFFh (I = FBh)");
    {
        Rig r;
        r.quiesce();
        r.load({0x00, 0x01}, 0xFBFF);                          // vector -> 0100h
        r.load({0x3E, 0xAA, 0x32, 0x00, 0x20, 0x76}, 0x0100);  // LD A,AAh / LD (2000h),A
        r.load({0x3E, 0xFB, 0xED, 0x47,                        // LD A,FBh / LD I,A
                0xED, 0x5E, 0xFB, 0x76},                       // IM 2 / EI / HALT
               0x0000);
        r.out(PTLATCH, 0x03);
        r.out(PTLATCH, ZRST | 0x03);
        r.run(MS);
        CHECK(r.sp->z80().halted() && r.sp->ram(0x32000) == 0x00, "waiting in HALT");
        // The host sets the counter as SPZ.CMD does: ACR B0h (counter, X1/X2 / 16), a short
        // count, IMR 08h (counter ready), then the start counter command (a read of FC3Eh).
        r.out(DUART + 0x4, 0xB0);
        r.out(DUART + 0x6, 0x00);
        r.out(DUART + 0x7, 0x10);
        r.out(DUART + 0x5, 0x08);
        (void)r.in(DUART + 0xE);
        r.run(MS);
        CHECK(r.sp->ram(0x32000) == 0xAA, "the handler at 0100h ran");
    }

    SECTION("spuz: board time = host time x 8 MHz / host hz()");
    {
        Rig r;
        r.quiesce();
        uint64_t b0 = r.sp->boardClock().now();
        r.run(1000);
        r.sp->catchUp();
        CHECK(r.sp->boardClock().now() - b0 == 4000, "1000 host T at 2 MHz = 4000 board T");
    }

    SECTION("spuz: sixteen boards share the window; SLAVESEL picks the one that answers");
    {
        Rig r(3);
        CHECK(r.m.bus.drain().empty(), "a SLAVESEL write to three boards is not contention");
        for (int i = 0; i < 3; ++i) {
            r.quiesce(3, (uint8_t)i);
            r.poke(0x5000, (uint8_t)(0xA0 + i));
        }
        for (int i = 0; i < 3; ++i)
            CHECK(r.board(i)->ram(0x35000) == 0xA0 + i, "each board got its own byte");
        r.out(SLAVESEL, 0x01);
        CHECK(r.peekWin(0x5000) == 0xA1 && r.board(1)->selected() && !r.board(0)->selected(),
              "select 1: board 1 answers the window");
        CHECK(r.m.bus.drain().empty(), "no contention on the shared ports");
    }

    SECTION("spuz: two boards with one select are refused");
    {
        Rig r(2);
        std::string why;
        CHECK(!r.m.tmaConflict(why), "selects 0 and 1: fine");
        setProperty(*r.board(1), "select", "0", err);
        CHECK(r.m.tmaConflict(why) && why.find("select 0") != std::string::npos,
              "selects 0 and 0: refused, and says why");
    }

    SECTION("spuz: the window pulls PHANTOM*, so RAM under it steps aside");
    {
        Rig r;
        r.m.add("memory", "mem", err);
        Board* mem = r.m.find("mem");
        setProperty(*mem, "ext", "true", err);
        setProperty(*mem, "honors_phantom", "all", err);
        CHECK(mem->loadSubUnit("region", {{"type", "ram"}, {"at", "000000"}, {"size", "1M"}}, err),
              ("1M of RAM from 0: " + err).c_str());
        r.m.power();
        r.quiesce();
        r.poke(0x0010, 0x5A);
        CHECK(r.peekWin(0x0010) == 0x5A && r.m.bus.drain().empty(),
              "the SPUZ answers its window, with no contention");
    }

    SECTION("spuz: a bus reset deselects the board and resets its Z80 (CLR*)");
    {
        Rig r;
        r.quiesce();
        r.m.reset(Reset::Bus);
        CHECK(r.sp->inReset() && !r.sp->selected() && (r.m.bus.viLines() & 0x01),
              "in reset, not selected, VI0 pulled");
    }

    SECTION("spuz: a snapshot taken mid-loop restores the whole private machine");
    {
        Rig r;
        r.start({0x21, 0x00, 0x00,        // LD HL,0
                 0x23, 0x7C, 0xFE, 0x40,  // INC HL / LD A,H / CP 40h
                 0x20, 0xFA,              // JR NZ,-6
                 0x22, 0x00, 0x30,        // LD (3000h),HL
                 0x76});
        r.run(MS);
        CHECK(!r.sp->z80().halted(), "the loop is running");
        const std::string snap = "spuz_snapshot.tmp";
        std::remove(snap.c_str());
        CHECK(r.m.snapshot(snap, err), "snapshot the machine mid-loop");
        r.run(100 * MS);
        uint64_t doneAt = r.sp->boardClock().now();
        CHECK(r.sp->z80().halted() && r.sp->ram(0x33001) == 0x40, "the original finishes");
        std::vector<std::string> notes;
        CHECK(r.m.restore(snap, err, notes) && notes.empty(), "restore it, whole");
        std::remove(snap.c_str());
        CHECK(!r.sp->z80().halted() && r.sp->ram(0x33001) == 0x00, "back mid-loop");
        r.run(100 * MS);
        CHECK(r.sp->z80().halted() && r.sp->ram(0x33001) == 0x40 &&
                  r.sp->boardClock().now() == doneAt,
              "the restored loop finishes at the same board T-state");
    }

    SECTION("spuz: units: the board Z80, and two serial lines a and b");
    {
        Rig r;
        auto u = r.sp->units();
        CHECK(u.size() == 3 && u[0].name == "z80" && u[0].kind == UnitKind::Cpu &&
                  u[0].state == "in reset",
              "z80 is a cpu unit, in reset after power-on");
        CHECK(u[1].name == "a" && u[2].name == "b" && u[1].kind == UnitKind::Serial &&
                  u[2].kind == UnitKind::Serial,
              "units a and b are serial lines");
        CHECK(r.sp->unitStream("a") != nullptr && r.sp->unitStream("c") == nullptr,
              "a has a stream, c does not exist");
    }

    SECTION("spuz: the board Z80 is a core: spuz0:z80 (stage 4)");
    {
        Rig r;
        std::string canon;
        CHECK(r.m.coreByRef("SPUZ0:Z80", &canon) == &r.sp->z80() && canon == "spuz0:z80",
              "coreByRef finds the private core, in any case");
        CHECK(r.m.privateOf(&r.sp->z80()) == &r.sp->z80Unit(), "privateOf maps it back");
        CHECK(r.m.coreTag(&r.sp->z80()) == "[spuz0:z80] ", "its register lines are tagged in full");
    }

    SECTION("spuz: a breakpoint on spuz0:z80 holds the board Z80 and stops the host run");
    {
        Rig r;
        addSpinningHost(r);
        r.start(kCountLoop);
        int id = r.m.debug.add(BreakKind::Pc, 0x0003, 0x0003);
        CHECK(r.m.debug.setCore(id, "spuz0:z80"), "bind the breakpoint to spuz0:z80");
        r.m.cpu()->setPc(0);
        RunResult rr = r.m.debug.run(200000);
        CHECK(rr.why == StopReason::Breakpoint && rr.bp == id && rr.bpCore == &r.sp->z80(),
              "the run stops on the private breakpoint, and names the core");
        CHECK(r.sp->z80().pc() == 0x0003 && r.sp->z80Unit().held(), "the board Z80 is held at 0003");
        const uint32_t a1 = zreg(r.sp, "A");
        const uint64_t b1 = r.sp->boardClock().now();
        r.m.clock.advance(10 * MS);  // host time passes: a held core does not move
        r.sp->catchUp();
        CHECK(r.sp->z80().pc() == 0x0003 && r.sp->boardClock().now() == b1,
              "held: neither the Z80 nor its clock moves");
        rr = r.m.debug.run(200000);
        CHECK(rr.why == StopReason::Breakpoint && r.sp->z80().pc() == 0x0003 &&
                  zreg(r.sp, "A") == ((a1 + 1) & 0xFF),
              "the next run goes past it once and stops there on the next lap");
        r.m.debug.clear();
        rr = r.m.debug.run(20000);
        CHECK(rr.why == StopReason::Steps && !r.sp->z80Unit().held(),
              "with the breakpoint gone, the next run lets the core go");
    }

    SECTION("spuz: STEP spuz0:z80 runs the board Z80 alone; in reset it is refused");
    {
        Rig r;
        std::string e;
        CHECK(!r.sp->z80Unit().step(e) && e.find("in reset") != std::string::npos,
              "in reset: refused, and says why");
        r.start(kCountLoop);
        r.run(MS);
        const uint64_t host = r.m.clock.now();
        const uint32_t pc0  = r.sp->z80().pc();
        CHECK(r.sp->z80Unit().step(e), "a step");
        CHECK(r.sp->z80().pc() != pc0 && r.m.clock.now() == host,
              "the board Z80 moved; host time did not");
    }

    SECTION("spuz: history=on records the board Z80; off records nothing");
    {
        Rig r;
        r.start(kCountLoop);
        r.run(MS);
        CHECK(r.sp->z80Unit().history(4).empty(), "off by default: nothing recorded");
        setProperty(*r.sp, "history", "on", err);
        r.run(MS);
        auto h = r.sp->z80Unit().history(4);
        bool inLoop = h.size() == 4;
        for (const auto& rec : h)
            inLoop = inLoop && rec.core == &r.sp->z80() && rec.pc >= 0x0002 && rec.pc <= 0x0003;
        CHECK(inLoop, "four records, all from the loop at 0002-0003");
        setProperty(*r.sp, "history", "off", err);
        CHECK(r.sp->z80Unit().history(4).empty(), "off clears the ring");
    }

    SECTION("spuz: the monitor takes spuz0:z80 in REGS, DISASM, DUMP, STEP, HISTORY, BREAK");
    {
        Rig r;
        Monitor m(r.m);
        r.start(kCountLoop);
        r.run(MS);
        CHECK(mon(m, "REGS spuz0:z80").rfind("[spuz0:z80] ", 0) == 0, "REGS: the tagged line");
        const std::string dis = mon(m, "DISASM spuz0:z80 0 3");
        CHECK(dis.find("INC") != std::string::npos && dis.find("JR") != std::string::npos,
              ("DISASM decodes the Z80's bank: " + dis).c_str());
        const std::string dump = mon(m, "DUMP spuz0:z80 0-4");
        CHECK(dump.find("3E 00 3C 18 FD") != std::string::npos,
              ("DUMP reads the Z80's bank: " + dump).c_str());
        const uint64_t host = r.m.clock.now();
        CHECK(mon(m, "STEP spuz0:z80").rfind("[spuz0:z80] ", 0) == 0 && r.m.clock.now() == host,
              "STEP: the board Z80 alone");
        CHECK(mon(m, "HISTORY CPU spuz0:z80").find("history=on") != std::string::npos,
              "HISTORY with recording off says how to turn it on");
        mon(m, "SET spuz0 history=on");
        r.run(MS);
        const std::string hist = mon(m, "HISTORY CPU spuz0:z80 2");
        size_t tags = 0;
        for (size_t at = hist.find("[spuz0:z80] "); at != std::string::npos;
             at = hist.find("[spuz0:z80] ", at + 1))
            ++tags;
        CHECK(tags == 2, ("HISTORY: two tagged records: " + hist).c_str());
        CHECK(mon(m, "BREAK spuz0:z80 3 IF A=1").find("no IF") != std::string::npos,
              "BREAK on a private core refuses IF");
        CHECK(mon(m, "REGS spuz0:nope").find("no core") != std::string::npos, "an unknown core");
    }

    SECTION("spuz: RUN reports a private breakpoint with both PCs");
    {
        Rig r;
        addSpinningHost(r);
        Monitor m(r.m);
        r.start(kCountLoop);
        mon(m, "BREAK spuz0:z80 3");
        const std::string out = mon(m, "RUN 0");
        CHECK(out.find("the board's core stopped at 0003") != std::string::npos,
              ("RUN names the board's core: " + out).c_str());
    }

    SECTION("spuz: STEP on a host core that is not active is refused");
    {
        Machine hm;
        Monitor m(hm);
        std::ostringstream sink;
        m.exec("BOARDS ADD cpu8588 cpu0", sink);
        CHECK(mon(m, "STEP cpu0:8088").find("not the processor that runs now") !=
                  std::string::npos,
              "the 8085 runs first: STEP cpu0:8088 is refused");
        CHECK(mon(m, "REGS cpu0:8088").rfind("[8088] ", 0) == 0, "...but REGS cpu0:8088 shows it");
    }
}
