// The 80286 core (src/cpu/cpu80286.cpp): its instruction set against tests recorded from a
// real 80286 (tests/cpu/80286/README.md), run through a real Bus with a memory board; then
// what the 80286 adds to the shared x86 core: its reset state, 24 address lines, the card's
// A20-A23, and its own faults. The hardware sample also checks the time (timing80286.h).

#include "boards/s100-memory.h"
#include "core/bus.h"
#include "core/machine.h"
#include "core/statefile.h"
#include "cpu/cpu80286.h"
#include "test.h"

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <memory>
#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace altair;

namespace {

std::vector<long> nums286(const std::string& s) {
    std::istringstream is(s);
    std::vector<long> v;
    long x;
    while (is >> x) v.push_back(x);
    return v;
}

const char* kRegs286[14] = {"AX", "BX", "CX", "DX", "CS", "SS", "DS", "ES",
                            "SP", "BP", "SI", "DI", "IP", "FLAGS"};

// RAM at 000000-10FFFF (real mode's reach) and 64K at FF0000 (where the 80286 starts).
struct Rig286 {
    Machine m;
    Cpu80286 cpu;
    std::map<std::string, RegDef> reg;
    Rig286() {
        std::string err;
        auto* mem = dynamic_cast<MemoryBoard*>(m.add("memory", "mem0", err));
        setProperty(*mem, "ext", "true", err);
        setProperty(*mem, "sixteen_bit", "true", err);   // SIXTN*: a word in one cycle, as in the suite
        Region r;
        r.kind = RegionKind::Ram;
        r.at = 0;
        r.size = 0x110000;
        mem->addRegion(r, err);
        Region top;
        top.kind = RegionKind::Ram;
        top.at = 0xFF0000;
        top.size = 0x10000;
        mem->addRegion(top, err);
        m.power();
        for (auto& d : cpu.registers()) reg[d.name] = d;
    }
    void set(const char* n, uint32_t v) { reg[n].set(v); }
    uint32_t get(const char* n) { return reg[n].get(); }
    void code(uint32_t at, std::initializer_list<uint8_t> b) {
        for (uint8_t x : b) m.bus.memWriteX(at++, x);
    }
    StepResult step() { return cpu.step(m.bus); }
};

// A harness board that pulls pINT and NMI* when told and answers the acknowledge with the
// bytes it is given (as in test_cpu8088.cpp).
class IrqBoard286 : public Board {
public:
    std::string type() const override { return "test-irq"; }
    std::vector<Property> properties() override { return {}; }
    bool assertsInt() const override { return int_; }
    bool assertsNmi() const override { return nmi_; }
    bool decodes(const BusCycle& c) const override { return c.type == Cycle::IntAck; }
    uint8_t read(const BusCycle& c) override {
        if (c.type != Cycle::IntAck) return 0xFF;
        ++acks;
        if (bytes.empty()) return 0xFF;
        uint8_t b = bytes.front();
        bytes.pop_front();
        return b;
    }
    void pullInt(bool on) { int_ = on; intChanged(); }
    void pullNmi(bool on) { nmi_ = on; nmiChanged(); }
    std::deque<uint8_t> bytes;
    int acks = 0;

private:
    bool int_ = false, nmi_ = false;
};

// Rig286 with the IRQ board, the registers at 0000:0100 and SS:SP = 0000:1000.
struct Rig286i : Rig286 {
    IrqBoard286* irq = nullptr;
    Rig286i() {
        irq = dynamic_cast<IrqBoard286*>(m.adopt(std::make_unique<IrqBoard286>()));
        set("CS", 0); set("IP", 0x100); set("SS", 0); set("SP", 0x1000);
        set("DS", 0); set("ES", 0); set("FLAGS", 0);
    }
    void vector(uint8_t type, uint16_t seg, uint16_t off, uint32_t table = 0) {
        code(table + type * 4u, {(uint8_t)off, (uint8_t)(off >> 8), (uint8_t)seg, (uint8_t)(seg >> 8)});
    }
};

} // namespace

void test_cpu80286() {
    SECTION("80286: the hardware sample -- registers and RAM against a real 80286");
    {
        Rig286 r;
        // P100_286_TIMING=<file>: a larger sample (the same format); prints, per instruction
        // form, the real count minus ours. A development aid, not a check.
        const char* alt = std::getenv("P100_286_TIMING");
        std::ifstream f(alt ? std::string(alt)
                            : std::string(ALTAIR_SOURCE_DIR) + "/tests/cpu/80286/sample.txt");
        CHECK(f.good(), "tests/cpu/80286/sample.txt opens");
        struct Diff { long n = 0, sum = 0, abs = 0, mn = 1 << 30, mx = -(1 << 30); };
        std::map<std::string, Diff> diffs;
        long tn = 0, within2 = 0;
        int exc = -1, cycles = 0;

        struct T {
            std::string name;
            long I[14] = {}, F[14] = {};
            std::vector<std::pair<uint32_t, int>> ram, fram;
        } t;
        long run = 0, pass = 0, mask = 0xFFFF;
        std::string stem, firstFail, line;
        std::vector<uint32_t> touched;

        auto runOne = [&] {
            ++run;
            for (uint32_t a : touched) r.m.bus.memWriteX(a, 0);
            touched.clear();
            for (auto& [a, v] : t.ram) {
                r.m.bus.memWriteX(a, (uint8_t)v);
                touched.push_back(a);
            }
            r.cpu.reset(Reset::PowerOn);
            for (int i = 0; i < 14; ++i) r.reg[kRegs286[i]].set((uint32_t)t.I[i]);
            long ours = 0;
            int guard = 0;
            do {
                ours += r.step().tStates;
            } while (r.cpu.repeating() && ++guard < 200000);
            // Time: no exception tests (their count runs through the vector fetch). The
            // suite starts each test after a jump (an empty queue) and ends it with HLT.
            static const char* dump = std::getenv("P100_286_DUMP");   // a form to list
            if (exc < 0 && dump && stem == dump) {
                std::printf("  DUMP d=%-4ld real=%-4d ours=%-4ld SP=%04lx SI=%04lx DI=%04lx CX=%04lx %s\n",
                            (long)cycles - ours, cycles, ours, t.I[8], t.I[10], t.I[11], t.I[2],
                            t.name.c_str());
            }
            if (exc < 0) {
                const long d = cycles - ours;
                Diff& x = diffs[stem];
                ++x.n; x.sum += d; x.abs += d < 0 ? -d : d;
                if (d < x.mn) x.mn = d;
                if (d > x.mx) x.mx = d;
            }

            std::string bad;
            for (int i = 0; i < 14; ++i) {
                long want = t.F[i] >= 0 ? t.F[i] : t.I[i];
                long got = r.reg[kRegs286[i]].get();
                if (i == 13) {
                    want &= mask;
                    got &= mask;
                }
                if (got != want) {
                    char b[64];
                    std::snprintf(b, sizeof b, " %s=%04lx want %04lx", kRegs286[i], got, want);
                    bad += b;
                }
            }
            for (auto& [a, v] : t.fram) {
                touched.push_back(a);
                const uint8_t g = r.m.bus.peekX(a);
                if (g == v) continue;
                if ((g & (mask & 0xFF)) == (v & (mask & 0xFF)) || (g & (mask >> 8)) == (v & (mask >> 8)))
                    continue;   // a pushed FLAGS image may differ only in undefined bits
                char b[64];
                std::snprintf(b, sizeof b, " [%06x]=%02x want %02x", a, g, v);
                bad += b;
            }
            if (bad.empty()) ++pass;
            else if (firstFail.empty()) firstFail = stem + " " + t.name + ":" + bad;
        };

        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            const std::string rest = line.size() > 2 ? line.substr(2) : "";
            switch (line[0]) {
            case 'F':
                if (line.rfind("FILE ", 0) == 0) {
                    std::istringstream is(line.substr(5));
                    is >> stem >> mask;
                } else {
                    auto v = nums286(rest);
                    for (int i = 0; i < 14; ++i) t.F[i] = v[i];
                }
                break;
            case 'N': t = T{}; t.name = rest; break;
            case 'I': {
                auto v = nums286(rest);
                for (int i = 0; i < 14; ++i) t.I[i] = v[i];
                break;
            }
            case 'R':
            case 'W': {
                auto v = nums286(rest);
                auto& dst = line[0] == 'R' ? t.ram : t.fram;
                for (long i = 0; i < v[0]; ++i) dst.push_back({(uint32_t)v[1 + 2 * i], (int)v[2 + 2 * i]});
                break;
            }
            case 'Q': {
                auto v = nums286(rest);
                exc = (int)v[0];
                cycles = (int)v[1];
                runOne();
                break;
            }
            default: break;
            }
        }
        // The overhead the suite adds to every test: the median of the per-form means.
        std::vector<double> means;
        for (auto& [k, x] : diffs) means.push_back(double(x.sum) / x.n);
        std::sort(means.begin(), means.end());
        const double base = means.empty() ? 0 : means[means.size() / 2];
        for (auto& [k, x] : diffs) {
            tn += x.n;
            const double m = double(x.sum) / x.n - base;
            if (m > -2.5 && m < 2.5) within2 += x.n;
            if (alt && (m <= -2.5 || m >= 2.5))
                std::printf("  TIMING %-6s n=%-4ld mean-base %+6.2f  min %ld max %ld\n", k.c_str(),
                            x.n, m, x.mn - (long)base, x.mx - (long)base);
        }
        std::printf("  time: suite overhead (median form) %.1f clocks; forms within +-2 of it: "
                    "%.1f%% of tests\n", base, 100.0 * within2 / (tn ? tn : 1));
        std::printf("  %ld/%ld tests give the real 80286's registers and RAM\n", pass, run);
        if (!firstFail.empty()) std::printf("  first failure: %s\n", firstFail.c_str());
        if (alt) return;   // the analysis run: no checks
        CHECK(run == 3912, "all 3912 sample tests ran");
        CHECK(pass == run, "every test gives the real chip's registers and RAM");
        // Time (src/cpu/timing80286.h): the suite adds a fixed overhead to every test (an
        // empty queue after its jump, the HLT that ends it), about 11 clocks. Against it,
        // 96.6% of the sample is within +-2 clocks. The bounds catch a broken table.
        CHECK(base >= 9.0 && base <= 13.0, "time: the suite's overhead is about 11 clocks");
        CHECK(within2 * 100 >= tn * 95, "time: 95% or more of the tests within +-2 clocks");
    }

    SECTION("80286: reset -- F000:FFF0 with a CS base of FF0000, first fetch at FFFFF0");
    {
        Rig286 r;
        r.cpu.reset(Reset::PowerOn);
        CHECK(r.get("CS") == 0xF000 && r.get("IP") == 0xFFF0, "CS:IP = F000:FFF0");
        CHECK(r.cpu.pc() == 0xFFFFF0 && r.cpu.csBase() == 0xFF0000, "PC FFFFF0, CS base FF0000");
        CHECK(r.get("DS") == 0 && r.get("SS") == 0 && r.get("ES") == 0 && r.get("FLAGS") == 0x0002,
              "DS = SS = ES = 0, flags clear (FLAGS 0002: bit 1 always reads 1)");
        // At FFFFF0: MOV AX,1234 then a far jump to 0000:0500, where HLT waits.
        r.code(0xFFFFF0, {0xB8, 0x34, 0x12, 0xEA, 0x00, 0x05, 0x00, 0x00});
        r.code(0x000500, {0xF4});
        r.step();
        CHECK(r.get("AX") == 0x1234, "the first instruction ran from FFFFF0 (A20-A23 high)");
        CHECK(r.cpu.pc() == 0xFFFFF3, "the PC is still in page FF before the far jump");
        r.step();
        CHECK(r.get("CS") == 0 && r.get("IP") == 0x500 && r.cpu.csBase() == 0, "the far jump: 0000:0500, base 0");
        r.step();
        CHECK(r.cpu.halted(), "HLT ran at 000500: A20-A23 are low after the far jump");
    }

    SECTION("80286: real mode reaches 10FFEF -- no wrap at 1 MB");
    {
        Rig286 r;
        r.set("CS", 0); r.set("IP", 0x100); r.set("DS", 0xFFFF); r.set("SP", 0x1000);
        r.code(0x100, {0xA0, 0x20, 0x00});             // MOV AL,[0020] (DS = FFFF: 100010)
        r.m.bus.memWriteX(0x100010, 0x5A);
        r.m.bus.memWriteX(0x000010, 0xA5);
        r.step();
        CHECK((r.get("AX") & 0xFF) == 0x5A, "FFFF:0020 reads 100010, not 000010");
    }

    SECTION("80286: the card's A20-A23 -- the memory manager replaces the lines in its mask");
    {
        Rig286 r;
        r.cpu.reset(Reset::PowerOn);
        r.code(0x0FFFF0, {0xB8, 0x77, 0x00});          // at 0FFFF0: MOV AX,0077
        r.code(0xFFFFF0, {0xB8, 0xFF, 0x00});          // at FFFFF0: MOV AX,00FF
        r.cpu.setHighAddress(0x0, 0xF);                // J3-J6 A-C, manager reset to 0
        r.step();
        CHECK(r.get("AX") == 0x77, "manager 0 on all four lines: the reset fetch is at 0FFFF0");
        r.cpu.reset(Reset::PowerOn);
        r.cpu.setHighAddress(0x0, 0x0);                // J3-J6 B-C: the 80286 drives them
        r.step();
        CHECK(r.get("AX") == 0xFF, "the 80286's own lines: the reset fetch is at FFFFF0");
    }

    SECTION("80286: its own faults -- LEA with a register operand is INT 6");
    {
        Rig286 r;
        r.set("CS", 0); r.set("IP", 0x100); r.set("SS", 0); r.set("SP", 0x1000);
        r.set("DS", 0); r.set("ES", 0); r.set("FLAGS", 0);
        r.code(0x100, {0x8D, 0xC1});                   // LEA AX,CX (mod 11)
        r.code(6 * 4, {0x00, 0x02, 0x00, 0x00});       // INT 6 -> 0000:0200
        r.step();
        CHECK(r.get("CS") == 0 && r.get("IP") == 0x200, "INT 6 taken, at 0000:0200");
        CHECK(r.get("SP") == 0x0FFA, "FLAGS, CS, IP pushed");
        const uint16_t ret = (uint16_t)(r.m.bus.peekX(0xFFA) | (r.m.bus.peekX(0xFFB) << 8));
        CHECK(ret == 0x100, "the pushed IP is the faulting instruction's");
    }

    SECTION("80286: INT with SP = 3 shuts down -- nothing pushed, CS:IP on the INT");
    {
        // Programmer's Reference, INT n: "the 80286 will shut down if the SP = 1, 3, or 5".
        Rig286i r;
        r.set("SP", 3);
        r.set("FLAGS", 0x0202);                         // IF
        r.code(0x100, {0xCD, 0x21});                    // INT 21
        r.vector(0x21, 0x2000, 0x0000);
        const StepResult s = r.step();
        CHECK(s.status == RunStatus::Halted && r.cpu.shutdown() && r.cpu.halted(),
              "the 80286 shuts down");
        CHECK(r.get("CS") == 0 && r.get("IP") == 0x100 && r.get("SP") == 3,
              "nothing pushed, CS:IP still on the INT");
        r.irq->bytes = {0xFF, 0x40};
        r.irq->pullInt(true);
        r.step();
        CHECK(r.irq->acks == 0 && r.cpu.shutdown(), "INTR is ignored in a shutdown");
        CHECK(!r.cpu.haltCanEnd(true, false), "and cannot end it");
        r.irq->pullInt(false);
        r.vector(2, 0x3000, 0x0000);
        r.irq->pullNmi(true);
        r.step();
        CHECK(r.cpu.shutdown() && r.get("SP") == 3, "NMI with SP still 3: shut down again");
        r.irq->pullNmi(false);
        r.step();
        r.set("SP", 0x1000);
        r.irq->pullNmi(true);
        r.step();
        CHECK(!r.cpu.shutdown() && !r.cpu.halted() && r.get("CS") == 0x3000,
              "NMI with room on the stack ends the shutdown (11.6)");
        r.set("SP", 1);
        r.code(0x30000, {0xCE});                        // INTO, OF clear: no interrupt
        r.step();
        CHECK(!r.cpu.shutdown(), "INTO with OF clear pushes nothing");
        r.code(0x30001, {0xCC});                        // INT 3
        r.step();
        CHECK(r.cpu.shutdown(), "INT 3 with SP = 1: shutdown");
        r.cpu.reset(Reset::Bus);
        CHECK(!r.cpu.shutdown() && !r.cpu.halted() && r.cpu.pc() == 0xFFFFF0, "RESET ends it");
    }

    SECTION("80286: a trap or an INTR that cannot push shuts down (no uncaught exception)");
    {
        Rig286i r;
        r.set("SP", 3);
        r.set("FLAGS", 0x0102);                         // TF
        r.code(0x100, {0x90, 0x90});
        const StepResult s = r.step();
        CHECK(s.status == RunStatus::Halted && r.cpu.shutdown(), "TF after NOP, SP = 3: shutdown");
        CHECK(r.get("IP") == 0x101 && r.get("SP") == 3, "the NOP ran; nothing pushed");

        Rig286i q;
        q.set("SP", 5);
        q.set("FLAGS", 0x0202);                         // IF
        q.code(0x100, {0x90});
        q.irq->bytes = {0xFF, 0x40};
        q.irq->pullInt(true);
        const StepResult t = q.step();
        CHECK(t.status == RunStatus::Halted && q.cpu.shutdown() && q.irq->acks == 2,
              "INTR acknowledged, SP = 5: shutdown");
        CHECK(q.get("IP") == 0x100 && q.get("SP") == 5, "nothing pushed");
    }

    SECTION("80286: setPc 100000-10FFEF is FFFF:IP (real mode reaches 10FFEF)");
    {
        Rig286 r;
        r.cpu.jumpFar(0x0000, 0x0100);
        r.cpu.setPc(0x100100);
        CHECK(r.get("CS") == 0xFFFF && r.get("IP") == 0x0110 && r.cpu.pc() == 0x100100,
              "100100 = FFFF:0110");
        CHECK(r.cpu.csBase() == 0xFFFF0, "CS base FFFF0, which a reload of CS keeps");
    }

    SECTION("80286: a segment register set by hand moves only its own base");
    {
        Rig286 r;
        r.cpu.reset(Reset::PowerOn);
        r.set("DS", 0x1234);
        r.set("SS", 0x2000);
        r.set("ES", 0x3000);
        CHECK(r.cpu.csBase() == 0xFF0000 && r.cpu.pc() == 0xFFFFF0,
              "DS, SS, ES: the reset CS base FF0000 stays");
        r.set("CS", 0xF000);
        CHECK(r.cpu.csBase() == 0xF0000, "CS set by hand: its base is CS*16");
    }

    SECTION("80286: segBase -- CS keeps its base, others are seg*16 to 10FFF0");
    {
        Rig286 r;
        r.cpu.reset(Reset::PowerOn);
        CHECK(r.cpu.segBase(0xF000) == 0xFF0000, "CS F000 after reset: FF0000");
        CHECK(r.cpu.segBase(0x1234) == 0x12340, "another segment: seg*16");
        CHECK(r.cpu.segBase(0xFFFF) == 0xFFFF0, "FFFF: FFFF0 (its last byte, FFFF:FFFF, is 10FFEF)");
    }

    SECTION("80286: 0F (two-byte opcode) gives no interrupt shadow; POP DS does");
    {
        Rig286i r;
        r.set("FLAGS", 0x0202);
        r.vector(0x60, 0x2000, 0x0000);
        r.code(0x100, {0x0F, 0x01, 0xE0, 0x90});        // SMSW AX, NOP
        r.step();
        r.irq->bytes = {0xFF, 0x60};   // the second INTA cycle reads the vector
        r.irq->pullInt(true);
        r.step();
        CHECK(r.irq->acks == 2 && r.get("CS") == 0x2000, "INTR taken right after SMSW");

        Rig286i q;
        q.set("FLAGS", 0x0202);
        q.vector(0x60, 0x2000, 0x0000);
        q.code(0x100, {0x1F, 0x90});                    // POP DS, NOP
        q.step();
        q.irq->bytes = {0xFF, 0x60};   // the second INTA cycle reads the vector
        q.irq->pullInt(true);
        q.step();
        CHECK(q.irq->acks == 0 && q.get("IP") == 0x102, "after POP DS the NOP runs first");
    }

    SECTION("80286: SNAPSHOT keeps the IDTR, the MSW and a shutdown");
    {
        Rig286i r;
        r.code(0x500, {0xFF, 0x03, 0x00, 0x10, 0x01, 0x00});   // limit 03FF, base 011000
        r.set("AX", 0xFFF2);
        r.code(0x100, {0x0F, 0x01, 0x1E, 0x00, 0x05,           // LIDT [0500]
                       0x0F, 0x01, 0xF0,                       // LMSW AX
                       0x0F, 0x01, 0xE3});                     // SMSW BX
        r.step(); r.step(); r.step();
        const uint32_t msw = r.get("BX");
        r.code(0x10B, {0x0F, 0x01, 0x0E, 0x00, 0x06});         // SIDT [0600]
        r.step();
        CHECK(r.m.bus.peekX(0x600) == 0xFF && r.m.bus.peekX(0x601) == 0x03 &&
                  r.m.bus.peekX(0x602) == 0x00 && r.m.bus.peekX(0x603) == 0x10 &&
                  r.m.bus.peekX(0x604) == 0x01,
              "LIDT [0500] then SIDT [0600]: limit 03FF, base 011000");
        r.set("IP", 0x10B);
        StateWriter w;
        r.cpu.serialize(w);

        Rig286i k;
        StateReader rd(w.data());
        k.cpu.deserialize(rd);
        CHECK(rd.ok() && rd.remaining() == 0, "the 80286 reads back to the last byte");
        k.code(0x10B, {0x0F, 0x01, 0xE1,                       // SMSW CX
                       0xCD, 0x21});                           // INT 21
        k.vector(0x21, 0x4000, 0x0000, 0x11000);               // in the table at 011000
        k.vector(0x21, 0x5000, 0x0000);                        // and a decoy at 0
        k.step();
        CHECK(k.get("CX") == msw, "the MSW travels");
        k.step();
        CHECK(k.get("CS") == 0x4000, "INT 21 reads the moved table: the IDTR travels");

        k.set("SP", 3);
        k.code(0x40000, {0xCC});
        k.step();
        StateWriter w2;
        k.cpu.serialize(w2);
        Cpu80286 other;
        StateReader rd2(w2.data());
        other.deserialize(rd2);
        CHECK(other.shutdown() && other.halted(), "a shutdown travels");
    }

    SECTION("80286: SNAPSHOT keeps the reset CS base");
    {
        Rig286 r;
        r.cpu.reset(Reset::PowerOn);
        r.cpu.setHighAddress(0x3, 0x5);
        StateWriter w;
        r.cpu.serialize(w);
        Cpu80286 other;
        other.jumpFar(0x1234, 0x10);
        StateReader rd(w.data());
        other.deserialize(rd);
        CHECK(other.pc() == 0xFFFFF0 && other.csBase() == 0xFF0000, "CS base FF0000 restored");
    }

    SECTION("80286: reset MSW = FFF0 (Programmer's Reference 10.4), real mode");
    {
        Rig286 r;
        r.cpu.reset(Reset::PowerOn);
        r.code(0xFFFFF0, {0x0F, 0x01, 0xE0});          // SMSW AX
        r.step();
        CHECK(r.get("AX") == 0xFFF0, "SMSW after reset reads FFF0");
    }

    SECTION("80286: time -- a taken jump makes the next instruction pay 1 clock per byte");
    {
        Rig286 r;
        r.set("CS", 0); r.set("IP", 0x100); r.set("SS", 0); r.set("SP", 0x1000);
        r.set("DS", 0); r.set("ES", 0); r.set("FLAGS", 0);
        r.code(0x100, {0xEB, 0x00, 0xB8, 0x34, 0x12, 0xB8, 0x34, 0x12});  // JMP $+2; MOV AX x2
        const uint32_t jmp = r.step().tStates;
        const uint32_t afterJump = r.step().tStates;
        const uint32_t plain = r.step().tStates;
        CHECK(jmp == 7, "JMP short: 7 clocks");
        CHECK(plain == 4, "MOV AX,imm: 2 clocks, but its 3 bytes take two 2-clock word fetches: 4");
        CHECK(afterJump == 2 + 3, "after the jump: 2 + 1 per byte of the instruction (3) = 5");
    }
}
