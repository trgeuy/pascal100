// The 8088 core (src/cpu/cpu8088.cpp): its instruction set, its bus cycles and its time,
// against tests recorded from a real 8088 (tests/cpu/8088/README.md), run through a real
// Bus with a 1 MB memory board -- every byte the core touches crosses the bus.

#include "boards/s100-memory.h"
#include "core/bus.h"
#include "core/machine.h"
#include "cpu/cpu8088.h"
#include "cpu/cpuZ80.h"
#include "test.h"

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <memory>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace altair;

namespace {

struct Hw {
    std::string name;
    std::vector<uint8_t> bytes;
    long I[14] = {}, F[14] = {};
    std::vector<std::pair<uint32_t, int>> ram, fram;
    int queue = 0, cycles = 0;
};

std::vector<long> nums(const std::string& s) {
    std::istringstream is(s);
    std::vector<long> v;
    long x;
    while (is >> x) v.push_back(x);
    return v;
}

const char* kRegs[14] = {"AX", "BX", "CX", "DX", "CS", "SS", "DS", "ES",
                         "SP", "BP", "SI", "DI", "IP", "FLAGS"};

// A harness board that pulls pINT and NMI* when told, and answers acknowledge cycles with
// the bytes it is given (0xFF, the floating bus, when it has none). It logs each acknowledge.
class IrqBoard : public Board {
public:
    std::string type() const override { return "test-irq"; }
    std::vector<Property> properties() override { return {}; }
    bool assertsInt() const override { return int_; }
    bool assertsNmi() const override { return nmi_; }
    bool decodes(const BusCycle& c) const override { return c.type == Cycle::IntAck && claim_; }
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
    bool claim_ = true;
    std::deque<uint8_t> bytes;
    int acks = 0;

private:
    bool int_ = false, nmi_ = false;
};

// A 1 MB machine (plus 64K at 100000 for the A20 test), an IrqBoard and a free-standing core.
struct Rig {
    Machine m;
    IrqBoard* irq = nullptr;
    Cpu8088 cpu;
    std::map<std::string, RegDef> reg;
    Rig() {
        std::string err;
        auto* mem = dynamic_cast<MemoryBoard*>(m.add("memory", "mem0", err));
        setProperty(*mem, "ext", "true", err);
        Region r;
        r.kind = RegionKind::Ram;
        r.at = 0;
        r.size = 0x110000;
        mem->addRegion(r, err);
        irq = dynamic_cast<IrqBoard*>(m.adopt(std::make_unique<IrqBoard>()));
        m.power();
        for (auto& d : cpu.registers()) reg[d.name] = d;
        set("CS", 0); set("IP", 0x100); set("SS", 0); set("SP", 0x1000);
        set("DS", 0); set("ES", 0); set("FLAGS", 0);
    }
    void set(const char* n, uint32_t v) { reg[n].set(v); }
    uint32_t get(const char* n) { return reg[n].get(); }
    void code(uint32_t at, std::initializer_list<uint8_t> b) {
        for (uint8_t x : b) m.bus.memWriteX(at++, x);
    }
    void vector(uint8_t type, uint16_t seg, uint16_t off) {
        m.bus.memWriteX(type * 4u, (uint8_t)off);
        m.bus.memWriteX(type * 4u + 1, (uint8_t)(off >> 8));
        m.bus.memWriteX(type * 4u + 2, (uint8_t)seg);
        m.bus.memWriteX(type * 4u + 3, (uint8_t)(seg >> 8));
    }
    uint16_t word(uint32_t a) { return (uint16_t)(m.bus.peekX(a) | (m.bus.peekX(a + 1) << 8)); }
    StepResult step() { return cpu.step(m.bus); }
};

} // namespace

void test_cpu8088() {
    SECTION("8088: the hardware sample -- registers, RAM and time against a real 8088");
    {
        Machine m;
        std::string err;
        auto* mem = dynamic_cast<MemoryBoard*>(m.add("memory", "mem0", err));
        CHECK(mem && setProperty(*mem, "ext", "true", err), "a 1 MB memory board, ext = true");
        Region r;
        r.kind = RegionKind::Ram;
        r.at = 0;
        r.size = 0x100000;
        CHECK(mem->addRegion(r, err), "RAM at 000000-0FFFFF");
        m.power();

        Cpu8088 cpu;
        std::map<std::string, RegDef> reg;
        for (auto& d : cpu.registers()) reg[d.name] = d;

        std::ifstream f(std::string(ALTAIR_SOURCE_DIR) + "/tests/cpu/8088/sample.txt");
        CHECK(f.good(), "tests/cpu/8088/sample.txt opens");

        long run = 0, pass = 0, preN = 0, preAbs = 0, preSum = 0, coldN = 0, coldSum = 0;
        std::string firstFail;
        std::vector<uint32_t> touched;
        std::string line, stem;
        long mask = 0xFFFF;
        Hw t;

        auto runOne = [&] {
            ++run;
            for (uint32_t a : touched) m.bus.memWriteX(a, 0);
            touched.clear();
            for (auto& [a, v] : t.ram) {
                m.bus.memWriteX(a, (uint8_t)v);
                touched.push_back(a);
            }
            cpu.reset(Reset::PowerOn);
            for (int i = 0; i < 14; ++i) reg[kRegs[i]].set((uint32_t)t.I[i]);

            long tStates = 0;
            int guard = 0;
            do {
                tStates += cpu.step(m.bus).tStates;
            } while (cpu.repeating() && ++guard < 200000);

            std::string bad;
            for (int i = 0; i < 14; ++i) {
                long want = t.F[i] >= 0 ? t.F[i] : t.I[i];
                long got = reg[kRegs[i]].get();
                if (i == 13) {
                    want &= mask;
                    got &= mask;
                }
                if (got != want) {
                    char b[64];
                    std::snprintf(b, sizeof b, " %s=%04lx want %04lx", kRegs[i], got, want);
                    bad += b;
                }
            }
            for (auto& [a, v] : t.fram) {
                touched.push_back(a);
                uint8_t g = m.bus.peekX(a);
                if (g == v) continue;
                // a pushed FLAGS image may differ only in undefined (masked) bits
                if ((g & (mask & 0xFF)) == (v & (mask & 0xFF)) || (g & (mask >> 8)) == (v & (mask >> 8)))
                    continue;
                char b[64];
                std::snprintf(b, sizeof b, " [%05x]=%02x want %02x", a, g, v);
                bad += b;
            }
            if (bad.empty()) ++pass;
            else if (firstFail.empty()) firstFail = stem + " " + t.name + ":" + bad;

            const long d = tStates - t.cycles;
            if (t.queue) {
                ++preN;
                preSum += d;
                preAbs += d < 0 ? -d : d;
            } else {
                ++coldN;
                coldSum += d;
            }
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
                    auto v = nums(rest);
                    for (int i = 0; i < 14; ++i) t.F[i] = v[i];
                }
                break;
            case 'N': t = Hw{}; t.name = rest; break;
            case 'I': {
                auto v = nums(rest);
                for (int i = 0; i < 14; ++i) t.I[i] = v[i];
                break;
            }
            case 'R':
            case 'W': {
                auto v = nums(rest);
                auto& dst = line[0] == 'R' ? t.ram : t.fram;
                for (long i = 0; i < v[0]; ++i)
                    dst.push_back({(uint32_t)v[1 + 2 * i], (int)v[2 + 2 * i]});
                break;
            }
            case 'Q': {
                auto v = nums(rest);
                t.queue = (int)v[0];
                t.cycles = (int)v[1];
                runOne();
                break;
            }
            default: break;
            }
        }

        std::printf("  %ld/%ld tests give the real 8088's registers and RAM\n", pass, run);
        if (!firstFail.empty()) std::printf("  first failure: %s\n", firstFail.c_str());
        CHECK(run == 3876, "all 3876 sample tests ran");
        CHECK(pass == run, "every test gives the real chip's registers and RAM");

        // Time, with the fetch floor (timing8088.h): about 2 clocks over the real chip from a
        // full prefetch queue, about 2 under from an empty one. The bounds catch a broken
        // table, not a clock of drift.
        const double preMean = double(preSum) / preN, preMae = double(preAbs) / preN;
        const double coldMean = double(coldSum) / coldN;
        std::printf("  time: full queue mean %+.2f |err| %.2f; empty queue mean %+.2f clocks\n",
                    preMean, preMae, coldMean);
        CHECK(preMean > 0.5 && preMean < 3.5, "full queue: our time is a little over the real chip's");
        CHECK(preMae < 4.0, "full queue: the mean error stays small");
        CHECK(coldMean < -0.5 && coldMean > -3.5, "empty queue: a little under the real chip's");
    }

    SECTION("8088: pINT -- two acknowledge cycles, the type on the second, then the vector");
    {
        Rig r;
        r.code(0x100, {0x90, 0x90});            // NOP NOP
        r.vector(0x42, 0x1234, 0x0010);
        r.code(0x12350, {0xF4});                // HLT at 1234:0010
        r.set("FLAGS", 0x0200);                 // IF
        r.irq->bytes = {0xFF, 0x42};
        r.irq->pullInt(true);
        StepResult s = r.step();
        CHECK(r.irq->acks == 2, "two acknowledge cycles");
        CHECK(r.get("CS") == 0x1234 && r.get("IP") == 0x0010, "CS:IP = the vector, 1234:0010");
        CHECK(r.get("SP") == 0x0FFA, "three words pushed");
        CHECK(r.word(0xFFA) == 0x0100 && r.word(0xFFC) == 0x0000 && r.word(0xFFE) == 0xF202,
              "the stack holds IP 0100, CS 0000, FLAGS F202 (bits 15-12 and 1 read as 1 on "
              "the 8088); the NOP did not run");
        CHECK((r.get("FLAGS") & 0x0300) == 0, "IF and TF are clear in the handler");
        CHECK(s.tStates == 61 + 4 * 5, "61 clocks + 4 per word moved (3 pushes, 2 vector words)");

        Rig q;
        q.code(0x100, {0x90});
        q.irq->pullInt(true);
        q.step();
        CHECK(q.irq->acks == 0 && q.get("IP") == 0x101, "IF clear: no acknowledge, the NOP runs");
    }

    SECTION("8088: MUL word -- FFFFh x FFFFh = FFFE0001h, no signed overflow on the way (CLK change 9)");
    {
        Rig r;
        r.code(0x100, {0xF7, 0xE3, 0xF7, 0xE3});   // MUL BX ; MUL BX
        r.set("AX", 0xFFFF); r.set("BX", 0xFFFF);
        r.step();
        CHECK(r.get("DX") == 0xFFFE && r.get("AX") == 0x0001, "DX:AX = FFFE:0001");
        CHECK((r.get("FLAGS") & 0x0801) == 0x0801, "CF and OF set: the high word is not 0");
        r.set("AX", 0x0002); r.set("BX", 0x7FFF);
        r.step();
        CHECK(r.get("DX") == 0 && r.get("AX") == 0xFFFE && (r.get("FLAGS") & 0x0801) == 0,
              "2 x 7FFFh = 0000:FFFE, CF and OF clear");
    }

    SECTION("8088: the interrupt shadow after STI and after MOV SS");
    {
        Rig r;
        r.code(0x100, {0xFB, 0x90, 0x90});      // STI ; NOP ; NOP
        r.vector(0x42, 0x2000, 0x0000);
        r.irq->bytes = {0xFF, 0x42};
        r.irq->pullInt(true);
        r.step();                                // STI
        CHECK(r.get("IP") == 0x101 && r.irq->acks == 0, "STI runs; no acknowledge yet");
        r.step();                                // the shadowed instruction
        CHECK(r.get("IP") == 0x102 && r.irq->acks == 0, "the instruction after STI runs first");
        r.step();
        CHECK(r.irq->acks == 2 && r.get("CS") == 0x2000, "then the interrupt is taken");

        Rig q;
        q.code(0x100, {0x8E, 0xD0, 0x90, 0x90}); // MOV SS,AX ; NOP ; NOP
        q.set("FLAGS", 0x0200);
        q.vector(0x42, 0x2000, 0x0000);
        q.irq->bytes = {0xFF, 0x42};
        q.irq->pullInt(true);
        q.step();                                // MOV SS -- an interrupt was pending at its start
        CHECK(q.irq->acks == 2, "pending before MOV SS: taken before it");
        Rig w;
        w.code(0x100, {0x8E, 0xD0, 0x90, 0x90});
        w.set("FLAGS", 0x0200);
        w.vector(0x42, 0x2000, 0x0000);
        w.step();                                // MOV SS
        w.irq->bytes = {0xFF, 0x42};
        w.irq->pullInt(true);
        w.step();                                // shadowed: the NOP runs
        CHECK(w.irq->acks == 0 && w.get("IP") == 0x103, "after MOV SS the next instruction runs");
        w.step();
        CHECK(w.irq->acks == 2, "and then the interrupt is taken");
    }

    SECTION("8088: NMI* -- edge triggered, vector 2, whatever IF says");
    {
        Rig r;
        r.code(0x100, {0x90, 0x90, 0x90, 0x90});
        r.vector(2, 0x3000, 0x0000);
        r.code(0x30000, {0xCF});                 // IRET
        r.irq->pullNmi(true);
        r.step();
        CHECK(r.get("CS") == 0x3000 && r.irq->acks == 0, "NMI: vector 2, no acknowledge cycle");
        r.step();                                // IRET
        CHECK(r.get("CS") == 0 && r.get("IP") == 0x100, "IRET returns to 0000:0100");
        r.step();
        CHECK(r.get("IP") == 0x101, "the line still held: no second NMI");
        r.irq->pullNmi(false);
        r.step();
        r.irq->pullNmi(true);
        r.step();
        CHECK(r.get("CS") == 0x3000, "released and pulled again: a new NMI");
    }

    SECTION("8088: TF -- INT 1 after the instruction");
    {
        Rig r;
        r.code(0x100, {0x90, 0x90});
        r.vector(1, 0x4000, 0x0000);
        r.set("FLAGS", 0x0100);
        r.step();
        CHECK(r.get("CS") == 0x4000, "INT 1 after the NOP");
        CHECK(r.word(0xFFA) == 0x0101, "the pushed IP is the next instruction");
        CHECK((r.get("FLAGS") & 0x0100) == 0, "TF is clear in the handler");
    }

    SECTION("8088: HLT -- waits; an interrupt wakes it and returns after the HLT");
    {
        Rig r;
        r.code(0x100, {0xF4, 0x90});
        r.vector(0x42, 0x2000, 0x0000);
        r.set("FLAGS", 0x0200);
        StepResult s = r.step();
        CHECK(s.status == RunStatus::Halted && r.cpu.halted(), "HLT halts");
        s = r.step();
        CHECK(s.status == RunStatus::Halted && r.get("IP") == 0x101, "still halted");
        r.irq->bytes = {0xFF, 0x42};
        r.irq->pullInt(true);
        s = r.step();
        CHECK(!r.cpu.halted() && r.get("CS") == 0x2000, "the interrupt ends the halt");
        CHECK(r.word(0xFFA) == 0x0101, "the pushed IP is after the HLT");
    }

    SECTION("8088: REP -- an interrupt between iterations, then the string finishes");
    {
        Rig r;
        r.code(0x100, {0xF3, 0xAA, 0x90});      // REP STOSB ; NOP
        r.vector(0x42, 0x2000, 0x0000);
        r.code(0x20000, {0xCF});                 // IRET
        r.set("FLAGS", 0x0200);
        r.set("CX", 10);
        r.set("DI", 0x500);
        r.set("AX", 0x5A);
        for (int i = 0; i < 3; ++i) r.step();
        CHECK(r.get("CX") == 7 && r.get("IP") == 0x100 && r.cpu.repeating(),
              "three iterations: CX 7, IP still on the REP prefix");
        r.irq->bytes = {0xFF, 0x42};
        r.irq->pullInt(true);
        r.step();
        CHECK(r.get("CS") == 0x2000 && r.word(0xFFA) == 0x0100,
              "the interrupt is taken between iterations; it returns to the prefix");
        r.irq->pullInt(false);
        r.step();                                // IRET
        int n = 0;
        while ((r.get("IP") != 0x102) && ++n < 50) r.step();
        CHECK(r.get("CX") == 0 && r.get("DI") == 0x50A, "the string finishes: CX 0, DI 050A");
        bool all = true;
        for (uint32_t a = 0x500; a < 0x50A; ++a) all = all && r.m.bus.peekX(a) == 0x5A;
        CHECK(all, "all ten bytes stored");
    }

    SECTION("8088: A20-A23 come from the card");
    {
        Rig r;
        r.code(0x100, {0xA2, 0x34, 0x12});      // MOV [1234],AL
        r.m.bus.memWriteX(0x001234, 0x00);     // the board's power-on fill is not zero
        r.m.bus.memWriteX(0x101234, 0x00);
        r.set("AX", 0x77);
        r.cpu.setHighAddress(0x1);
        r.code(0x100100, {0xA2, 0x34, 0x12});   // the same code, in the 100000 segment
        r.step();
        CHECK(r.m.bus.peekX(0x101234) == 0x77 && r.m.bus.peekX(0x001234) == 0x00,
              "high nibble 1: the fetch and the store are at 1xxxxx");
    }

    SECTION("8088: the debugger PC is the linear address CS*16+IP");
    {
        Rig r;
        CHECK(r.cpu.pcMask() == 0xFFFFF, "pcMask is 20 bits");
        r.set("CS", 0x1234); r.set("IP", 0x0010);
        CHECK(r.cpu.pc() == 0x12350, "pc() = CS*16 + IP");
        r.set("CS", 0xFFFF); r.set("IP", 0x0010);
        CHECK(r.cpu.pc() == 0x00000, "pc() wraps at 1M, as the 8088's 20 lines do");

        // Inside the 64K that CS covers: CS stays, IP moves.
        r.set("CS", 0x1000); r.set("IP", 0);
        r.cpu.setPc(0x1ABCD);
        CHECK(r.get("CS") == 0x1000 && r.get("IP") == 0xABCD, "setPc in CS's 64K keeps CS");
        // Outside it: CS = address/16, IP = address%16.
        r.cpu.setPc(0x54321);
        CHECK(r.get("CS") == 0x5432 && r.get("IP") == 0x0001, "setPc past CS's 64K loads CS");
        // CS near the top: its 64K wraps past FFFFF to 0, and setPc follows it.
        r.set("CS", 0xFFF0); r.set("IP", 0);
        r.cpu.setPc(0x00050);
        CHECK(r.get("CS") == 0xFFF0 && r.get("IP") == 0x0150, "setPc follows CS's wrap at 1M");

        // A setPc that loads CS must also move the fetch: the next step runs there.
        r.code(0x54321, {0x90});                 // NOP
        r.set("CS", 0); r.set("IP", 0x100);
        r.cpu.setPc(0x54321);
        r.step();
        CHECK(r.cpu.pc() == 0x54322, "the step after setPc fetched at the new CS");

        // Messages give CS:IP and the linear address; a flat core keeps four digits.
        r.set("CS", 0x1234); r.set("IP", 0x0010);
        CHECK(pcText(r.cpu) == "1234:0010 (12350)", "pcText on the 8088: CS:IP (linear)");
        CpuZ80 z;
        z.setPc(0x0100);
        uint16_t sg, of;
        CHECK(!z.pcSegOff(sg, of) && pcText(z) == "0100", "pcText on a flat core: 0100");
    }

    SECTION("8088: CpuCard::peekCodeByte reads past 64K");
    {
        struct OneCore : CpuCard {
            CpuCore* c;
            explicit OneCore(CpuCore* x) : c(x) {}
            CpuCore* activeCore() override { return c; }
        };
        Rig r;
        r.m.bus.memWriteX(0x0ABCD, 0x11);
        r.m.bus.memWriteX(0x3ABCD, 0x33);
        OneCore card(&r.cpu);
        CHECK(card.peekCodeByte(&r.cpu, r.m.bus, 0x3ABCD) == 0x33,
              "the byte at 3ABCD, not at ABCD in segment 0");
        CHECK(card.peekCodeByte(&r.cpu, r.m.bus, 0x0ABCD) == 0x11, "below 64K is unchanged");
    }

    SECTION("8086: the 16-bit data bus (setWideBus) -- one cycle, byte-serial, odd address");
    {
        // The data cycles at 0200-0203 that the step ran: the fetches (at 0100) are left out.
        struct Seen { uint16_t addr; bool write, xtrq; uint8_t lo, hi; };
        auto run = [](bool wide, bool sixteen, std::initializer_list<uint8_t> code,
                      std::vector<Seen>& seen, uint32_t& t, Rig*& keep) {
            static std::unique_ptr<Rig> r;
            r = std::make_unique<Rig>();
            std::string err;
            setProperty(*r->m.find("mem0"), "sixteen_bit", sixteen ? "true" : "false", err);
            r->cpu.setWideBus(wide);
            r->code(0x100, code);
            r->m.bus.memWriteX(0x0200, 0x34);
            r->m.bus.memWriteX(0x0201, 0x12);
            r->m.bus.memWriteX(0x0202, 0x56);
            r->set("AX", 0xBEEF);
            seen.clear();
            int h = r->m.bus.observe([&seen](const BusCycle& c) {
                if ((c.type == Cycle::MemRead || c.type == Cycle::MemWrite) && c.addr >= 0x0200 &&
                    c.addr <= 0x0203)
                    seen.push_back({c.addr, c.type == Cycle::MemWrite, c.xtrq, c.data, c.dataHi});
            });
            t = r->step().tStates;
            r->m.bus.unobserve(h);
            keep = r.get();
        };
        std::vector<Seen> s;
        uint32_t t = 0;
        Rig* r = nullptr;

        run(true, true, {0xA1, 0x00, 0x02}, s, t, r);  // MOV AX,[0200]
        CHECK(std::string(r->cpu.isa()) == "8086", "setWideBus(true): the core is an 8086");
        CHECK(s.size() == 1 && s[0].addr == 0x0200 && s[0].xtrq && s[0].lo == 0x34 &&
                  s[0].hi == 0x12 && r->get("AX") == 0x1234,
              "an even word from 16-bit RAM: ONE cycle, sXTRQ*, both bytes");
        const uint32_t t86 = t;

        run(true, false, {0xA1, 0x00, 0x02}, s, t, r);
        CHECK(s.size() == 2 && s[0].addr == 0x0200 && s[1].addr == 0x0201 && !s[0].xtrq &&
                  r->get("AX") == 0x1234,
              "...from 8-bit RAM: the byte-serial pair, the EVEN byte first");
        CHECK(t == t86 + 4, "...and the second cycle costs 4 clocks");

        run(true, true, {0xA1, 0x01, 0x02}, s, t, r);  // MOV AX,[0201]
        CHECK(s.size() == 2 && s[0].addr == 0x0201 && s[1].addr == 0x0202 && !s[0].xtrq &&
                  r->get("AX") == 0x5612,
              "an odd word, even from 16-bit RAM: two byte cycles, low byte first");

        run(true, true, {0xA3, 0x00, 0x02}, s, t, r);  // MOV [0200],AX
        CHECK(s.size() == 1 && s[0].write && s[0].xtrq && r->word(0x0200) == 0xBEEF,
              "an even word write to 16-bit RAM: ONE cycle");
        run(true, false, {0xA3, 0x00, 0x02}, s, t, r);
        CHECK(s.size() == 2 && s[0].addr == 0x0200 && s[1].addr == 0x0201 && s[0].write &&
                  r->word(0x0200) == 0xBEEF,
              "...to 8-bit RAM: two writes, the even byte first");

        run(false, true, {0xA1, 0x00, 0x02}, s, t, r);
        CHECK(std::string(r->cpu.isa()) == "8088" && s.size() == 2 && !s[0].xtrq &&
                  t == t86 + 4,
              "the 8088 (the default) is unchanged: two byte cycles even from 16-bit RAM, +4");
    }
}
