#include "test.h"

#include "boards/compupro-mdriveh.h"
#include "cli/monitor.h"
#include "config/toml.h"
#include "core/board.h"
#include "core/machine.h"
#include "core/statefile.h"

#include <sstream>
#include <string>
#include <vector>

using namespace altair;

// CompuPro M-Drive/H (reference/CompuPro M-Drive H.md). Every check drives real bus cycles,
// so the port decode, the shared-port rule and the floating bus are tested with the board.

namespace {

const char* kOneBoard = R"(
[machine]
name = "mdh1"

[[board]]
type = "mdriveh"
id   = "mdh0"
fill = "zero"
)";

// The 1st and 2nd boards of a system: the same two ports, cascade 0 and 1.
const char* kTwoBoards = R"(
[machine]
name = "mdh2"

[[board]]
type = "mdriveh"
id   = "mdh0"
fill = "zero"

[[board]]
type    = "mdriveh"
id      = "mdh1"
fill    = "zero"
cascade = 1
)";

struct Rig {
    Machine                m;
    CompuProMDriveHBoard*  a = nullptr;
    CompuProMDriveHBoard*  b = nullptr;

    explicit Rig(const char* toml) {
        std::string err;
        CHECK(loadTomlText(toml, "mdh", m, err), "an M-Drive/H machine file loads");
        a = dynamic_cast<CompuProMDriveHBoard*>(m.find("mdh0"));
        b = dynamic_cast<CompuProMDriveHBoard*>(m.find("mdh1"));
        CHECK(a != nullptr, "the registry makes an mdriveh board");
        m.power();
    }
    uint8_t in(uint8_t p) { return m.bus.ioRead(p); }
    void    out(uint8_t p, uint8_t v) { m.bus.ioWrite(p, v); }
    // As the manual's SETMDRV does: A, then H, then L to the address port.
    void addr(uint32_t a22) {
        out(0xC7, (uint8_t)(a22 >> 16));
        out(0xC7, (uint8_t)(a22 >> 8));
        out(0xC7, (uint8_t)a22);
    }
};

// The manual's sample driver, one sector: byte address (sector-1)*64K + track*128, 128 data
// bytes, then a parity byte at (sector-1)*64K + track. The sum starts at 128 (MVI C,128;
// MOV B,C) and adds each byte. Returns true when a read's parity matches.
bool writeSector(Rig& g, int track, int sector, const std::vector<uint8_t>& d) {
    g.addr((uint32_t)(sector - 1) << 16 | (uint32_t)track * 128);
    uint8_t sum = 128;
    for (uint8_t v : d) { g.out(0xC6, v); sum = (uint8_t)(sum + v); }
    g.addr((uint32_t)(sector - 1) << 16 | (uint32_t)track);
    g.out(0xC6, sum);
    return true;
}
bool readSector(Rig& g, int track, int sector, std::vector<uint8_t>& d) {
    g.addr((uint32_t)(sector - 1) << 16 | (uint32_t)track * 128);
    d.assign(128, 0);
    uint8_t sum = 128;
    for (auto& v : d) { v = g.in(0xC6); sum = (uint8_t)(sum + v); }
    g.addr((uint32_t)(sector - 1) << 16 | (uint32_t)track);
    return (uint8_t)(g.in(0xC6) - sum) == 0;
}

// CompuPro CP/M 2.2N's HMDTEST: per board, read a sector, write it back, read it again; a
// clean second read means the board is there. 8 sectors per board, up to 8 boards.
int sizeBoards(Rig& g) {
    int boards = 0;
    for (int k = 0; k < 8; ++k) {
        std::vector<uint8_t> d;
        const int sector = 8 * k + 1;
        readSector(g, 4, sector, d);
        writeSector(g, 4, sector, d);
        if (!readSector(g, 4, sector, d)) break;
        ++boards;
    }
    return boards;
}

} // namespace

void test_mdriveh() {
    std::printf("\n-- CompuPro M-Drive/H --\n");

    // ----- Port map and the shifting address load -----
    {
        Rig g(kOneBoard);
        if (!g.a) return;
        CHECK(g.a->counter() == 0, "the counter starts at 0 after power");
        g.out(0xC7, 0x12);
        g.out(0xC7, 0x34);
        g.out(0xC7, 0x56);
        CHECK(g.a->counter() == 0x123456, "three OUTs to C7, high byte first, load 123456");
        g.out(0xC7, 0x78);
        CHECK(g.a->counter() == 0x345678, "a fourth OUT shifts the first byte out (22 bits: 345678)");
        g.addr(0xFF0000);
        CHECK(g.a->counter() == 0x3F0000, "the top two bits of the high byte are lost");

        g.addr(0x000100);
        g.out(0xC6, 0xA5);
        g.out(0xC6, 0x5A);
        CHECK(g.a->counter() == 0x000102, "each data OUT counts up by one");
        CHECK(g.a->byteAt(0x100) == 0xA5 && g.a->byteAt(0x101) == 0x5A, "and stores the byte at the counter");
        g.addr(0x000100);
        CHECK(g.in(0xC6) == 0xA5 && g.in(0xC6) == 0x5A, "IN C6 reads the bytes back in order");
        CHECK(g.a->counter() == 0x000102, "each data IN counts up by one");
        CHECK(!g.m.bus.lastContended(), "one board is never a contention");

        CHECK(g.in(0xC7) == 0xFF && g.m.bus.lastUnclaimed(), "the address port is write-only: an IN floats to FF");
        CHECK(g.a->counter() == 0x000102, "and does not count");
    }

    // ----- Moving the base moves both ports; the base is even -----
    {
        Rig g(kOneBoard);
        if (!g.a) return;
        std::string err;
        CHECK(!setProperty(*g.a, "port", "0xC7", err), "an odd port is refused");
        CHECK(err.find("even") != std::string::npos, "and the message says the port must be even");
        CHECK(setProperty(*g.a, "port", "0x40", err), "port 40 is accepted");
        g.out(0x41, 0);
        g.out(0x41, 0);
        g.out(0x41, 7);
        g.out(0x40, 0x99);
        CHECK(g.a->byteAt(7) == 0x99, "the address port moved to 41 and the data port to 40");
        g.out(0xC6, 0x11);
        CHECK(g.m.bus.lastUnclaimed(), "C6 is no longer decoded");
        const auto map = g.a->ioMap();
        CHECK(map.size() == 2 && map[0].lo == 0x40 && map[1].lo == 0x41, "the I/O map names 40 and 41");
    }

    // ----- One board: past its 512K the bus floats -----
    {
        Rig g(kOneBoard);
        if (!g.a) return;
        g.addr(0x07FFFF);
        CHECK(g.a->selected(), "cascade 0 holds 000000-07FFFF");
        g.out(0xC6, 0x42);
        CHECK(!g.a->selected() && g.a->counter() == 0x080000, "the counter moves on to 080000, outside the board");
        CHECK(g.in(0xC6) == 0xFF, "a read there floats to FF: no board drives it");
        CHECK(!g.m.bus.lastUnclaimed(), "the port is still decoded (every board counts)");
        CHECK(g.a->counter() == 0x080001, "and the read counted");
        g.out(0xC6, 0x77);
        CHECK(g.a->byteAt(1) == 0, "a write at 080001 is stored by no one (not wrapped onto the board's byte 1)");
        g.addr(0x080001);
        CHECK(g.in(0xC6) == 0xFF, "so it reads back as the floating bus");
        g.addr(0x3FFFFF);
        g.in(0xC6);
        CHECK(g.a->counter() == 0, "the counter wraps from 3FFFFF to 0");
    }

    // ----- Two boards on the same ports -----
    {
        Rig g(kTwoBoards);
        if (!g.a || !g.b) return;
        g.addr(0x07FFFE);
        CHECK(!g.m.bus.lastContended(), "an address OUT taken by two M-Drive/H boards is not a contention");
        CHECK(g.m.bus.drain().empty(), "and nothing is reported");
        for (int i = 0; i < 4; ++i) g.out(0xC6, (uint8_t)(0x10 + i));
        CHECK(!g.m.bus.lastContended(), "neither is a data OUT");
        CHECK(g.a->counter() == 0x080002 && g.b->counter() == 0x080002, "both boards count every byte");
        CHECK(g.a->byteAt(0x7FFFE) == 0x10 && g.a->byteAt(0x7FFFF) == 0x11, "the 1st board stores the bytes below 512K");
        CHECK(g.b->byteAt(0) == 0x12 && g.b->byteAt(1) == 0x13, "the 2nd board takes over at 080000");
        g.addr(0x07FFFE);
        uint8_t v[4];
        for (auto& x : v) x = g.in(0xC6);
        CHECK(v[0] == 0x10 && v[1] == 0x11 && v[2] == 0x12 && v[3] == 0x13, "a read across the boundary reads both boards");
        CHECK(!g.m.bus.lastContended() && g.m.bus.drain().empty(), "only one board drives each read");
        g.addr(0x100000);
        CHECK(g.in(0xC6) == 0xFF, "the 3rd board's range has no board: FF");
        CHECK(sizeBoards(g) == 2, "CompuPro's read/write/read sizing finds two boards");

        Monitor mon(g.m);
        std::ostringstream o;
        mon.exec("SHOW BUS CONTENTION", o);
        CHECK(o.str().find("none.") != std::string::npos,
              ("SHOW BUS CONTENTION does not list two M-Drive/H boards: " + o.str()).c_str());
    }

    // ----- Misstrapped boards are still reported -----
    {
        Rig g(R"(
[machine]
name = "mdhbad"

[[board]]
type = "mdriveh"
id   = "mdh0"

[[board]]
type = "mdriveh"
id   = "mdh1"
)");
        if (!g.a || !g.b) return;
        g.m.bus.drain();
        g.addr(0);
        g.in(0xC6);
        CHECK(g.m.bus.lastContended(), "two boards strapped to the same cascade both drive a read: contention");
    }
    {
        Rig g(R"(
[machine]
name = "mdhif1"

[[board]]
type = "mdriveh"
id   = "mdh0"

[[board]]
type = "interfacer1"
id   = "if1"

  [board.unit.a]
  port = C6
)");
        if (!g.a) return;
        g.m.bus.drain();
        g.out(0xC7, 0);
        CHECK(g.m.bus.lastContended(), "an Interfacer 1 at C6 does not share the port: a write is contention");
        CHECK(!g.m.bus.drain().empty(), "and it is reported");
        Monitor mon(g.m);
        std::ostringstream o;
        mon.exec("SHOW BUS CONTENTION", o);
        CHECK(o.str().find("port C7 OUT driven by mdh0 if1") != std::string::npos,
              ("SHOW BUS CONTENTION lists the M-Drive/H and the Interfacer 1: " + o.str()).c_str());
    }

    // ----- Sizing with one board, and with the 1st board missing -----
    {
        Rig g(kOneBoard);
        if (!g.a) return;
        CHECK(sizeBoards(g) == 1, "the sizing routine finds one board");
        std::string err;
        CHECK(setProperty(*g.a, "fill", "random", err), "fill=random is accepted");
        g.m.power();
        CHECK(sizeBoards(g) == 1, "and still finds it when the RAM powers up random");
        CHECK(setProperty(*g.a, "cascade", "1", err), "the only board strapped as the 2nd");
        CHECK(sizeBoards(g) == 0, "no 1st board: the parity check fails at once, 0 boards");
        CHECK(g.m.bus.drain().empty(), "and the empty range reports nothing");
    }

    // ----- Reset, power, fill and the seed -----
    {
        Rig g(kOneBoard);
        if (!g.a) return;
        g.addr(0x1234);
        g.out(0xC6, 0x66);
        g.m.reset(Reset::Bus);
        CHECK(g.a->counter() == 0x1235 && g.a->byteAt(0x1234) == 0x66, "RESET keeps the counter and the RAM");
        g.m.power();
        CHECK(g.a->counter() == 0 && g.a->byteAt(0x1234) == 0, "power with fill=zero clears the RAM");
        std::string err;
        CHECK(setProperty(*g.a, "fill", "random", err), "fill=random");
        g.m.power();
        bool allFF = true;
        uint32_t sum = 0;
        for (uint32_t i = 0; i < 4096; ++i) { allFF = allFF && g.a->byteAt(i) == 0xFF; sum += g.a->byteAt(i); }
        CHECK(!allFF && sum != 0, "a random fill is not the floating bus and not zero");
        const uint8_t first = g.a->byteAt(0), mid = g.a->byteAt(0x40000);
        g.m.power();
        CHECK(g.a->byteAt(0) == first && g.a->byteAt(0x40000) == mid, "the same seed fills the same way");
        CHECK(setProperty(*g.a, "seed", "7", err), "a new seed is accepted");
        g.m.power();
        CHECK(g.a->byteAt(0) != first || g.a->byteAt(0x40000) != mid, "and fills differently");
    }

    // ----- Snapshot and properties -----
    {
        Rig g(kOneBoard);
        if (!g.a) return;
        g.addr(0x7FFFE);
        g.out(0xC6, 0xC3);
        g.out(0xC6, 0x3C);
        StateWriter w;
        g.a->serialize(w);
        Rig h(kOneBoard);
        if (!h.a) return;
        StateReader r(w.data());
        h.a->deserialize(r);
        CHECK(h.a->counter() == 0x80000 && h.a->byteAt(0x7FFFE) == 0xC3 && h.a->byteAt(0x7FFFF) == 0x3C,
              "a snapshot keeps the counter and the RAM");

        std::string err;
        CHECK(!setProperty(*g.a, "cascade", "8", err), "cascade 8 is refused (S1-8..10: 0-7)");
        CHECK(setProperty(*g.a, "cascade", "7", err), "cascade 7 is accepted");
        CHECK(!setProperty(*g.a, "fill", "ff", err), "fill=ff is refused");
        const std::string out = saveTomlText(g.m);
        CHECK(out.find("cascade = 7") != std::string::npos, "CONFIG SAVE writes the cascade");
        Machine back;
        CHECK(loadTomlText(out, "mdh-back", back, err), "and the saved file loads again");
        auto* c = dynamic_cast<CompuProMDriveHBoard*>(back.find("mdh0"));
        CHECK(c != nullptr, "with the M-Drive/H in it");
    }
}
