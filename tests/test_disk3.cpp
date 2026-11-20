// CompuPro DISK 3 (src/boards/compupro-disk3.h), through real bus cycles. The host is a Z80
// that does what CompuPro's CP/M 8-16 V1.1PD BIOS does (GBCBIOSA.A86 D3INIT, GBCBIOS6.A86):
// write the LINK into the default IOPB at 50h, OUT 01 (reset) and OUT 00 (warm-up attention)
// to port 90, then for each command clear STATUS, OUT 00, and poll STATUS until it is not 0.
// The board's DMA moves in the run loop as the bus is granted.

#include "boards/compupro-disk3.h"
#include "boards/s100-memory.h"
#include "core/bus.h"
#include "core/debug.h"
#include "core/machine.h"
#include "core/statefile.h"
#include "host/media.h"
#include "test.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace altair;

namespace {

constexpr uint8_t  PORT = 0x90;
constexpr uint16_t IOPB = 0x1000, IOPB2 = 0x1010, BUF = 0x2000, PARM = 0x3000;

// A small drive keeps the tests fast: 4 cylinders x 2 heads x 9 sectors x 1024 bytes.
constexpr int      kCyl = 4, kHeads = 2, kSec = 9, kSize = 1024;
constexpr uint64_t kBytes = (uint64_t)kCyl * kHeads * kSec * kSize;
const KeyValues    kShape = {{"cylinders", "4"}, {"heads", "2"}};

// IOPB bytes (p. 11) and commands (p. 14).
constexpr int CMD = 0, STAT = 1, DRV = 2, ARG1 = 3, DATA = 10, LINK = 13;
constexpr uint8_t NOOP = 0, VERSION = 1, GLOBAL = 2, SPECIFY = 3, SETMAP = 4, HOME = 5, SEEK = 6,
                  RW = 8, FORMAT = 0x0A, EXAMINE = 0x0E;

// Every byte of a disk is its own file offset, stamped, so a sector from the wrong place never
// matches by accident.
uint8_t stamp(uint64_t i) { return (uint8_t)((i + i / 1024 * 7) & 0xFF); }

uint64_t offsetOf(int physTrack, int sec) {
    return ((uint64_t)physTrack * kSec + (uint64_t)sec) * kSize;
}

void withDisk(uint64_t bytes) {
    setMediaResolver([bytes](const std::string& path, bool ro, std::string&) {
        std::vector<uint8_t> d((size_t)bytes);
        for (size_t i = 0; i < d.size(); ++i) d[i] = stamp(i);
        return std::make_unique<MemoryMedia>(path, std::move(d), ro);
    });
}

struct Rig {
    Machine             m;
    CompuProDisk3Board* hd = nullptr;

    Rig() {
        std::string err;
        auto* mem = dynamic_cast<MemoryBoard*>(m.add("memory", "mem0", err));
        Region r;
        r.kind = RegionKind::Ram;
        r.at   = 0x0000;
        r.size = 0x10000;
        mem->addRegion(r, err);
        setProperty(*mem, "fill", "zero", err);
        mem->power();
        hd = dynamic_cast<CompuProDisk3Board*>(m.add("disk3", "hd0", err));
        m.add("z80", "cpu0", err);
        m.cpu()->reset(Reset::PowerOn);
        hd->power();
        // The default IOPB's LINK points at our IOPB, which points at itself (D3INIT).
        link(0x50, IOPB);
        link(IOPB, IOPB);
    }

    uint8_t mem(uint16_t a) { return m.bus.memRead(a); }
    void    poke(uint16_t a, uint8_t v) { m.bus.memWrite(a, v); }
    uint16_t word(uint16_t a) { return (uint16_t)(mem(a) | mem((uint16_t)(a + 1)) << 8); }
    void    pokeWord(uint16_t a, uint16_t v) {
        poke(a, (uint8_t)v);
        poke((uint16_t)(a + 1), (uint8_t)(v >> 8));
    }
    uint32_t addr3(uint16_t a) { return (uint32_t)word(a) | (uint32_t)mem((uint16_t)(a + 2)) << 16; }
    void link(uint16_t at, uint32_t to) {
        pokeWord((uint16_t)(at + LINK), (uint16_t)to);
        poke((uint16_t)(at + LINK + 2), (uint8_t)(to >> 16));
    }

    // Run Z80 code at 8000 to its HLT.
    void runCode(std::vector<uint8_t> code) {
        uint16_t pc = 0x8000;
        for (uint8_t b : code) poke(pc++, b);
        m.cpu()->reset(Reset::Bus);
        m.cpu()->setPc(0x8000);
        m.debug.run(0);
    }

    // D3INIT's start: OUT 01 (reset board 1), OUT 00 (the warm-up attention).
    void resetAndWarmUp() { runCode({0x3E, 0x01, 0xD3, PORT, 0x3E, 0x00, 0xD3, PORT, 0x76}); }

    // An attention, then poll the STATUS byte of the IOPB at `at` until it is not 0:
    //   8000 MVI A,0 / OUT 90 / 8004 LDA at+1 / ORA A / JZ 8004 / HLT
    uint8_t attn(uint16_t at = IOPB) {
        poke((uint16_t)(at + STAT), 0);
        const uint16_t s = (uint16_t)(at + STAT);
        runCode({0x3E, 0x00, 0xD3, PORT, 0x3A, (uint8_t)s, (uint8_t)(s >> 8), 0xB7, 0xCA, 0x04, 0x80,
                 0x76});
        return mem(s);
    }

    // Fill in the IOPB and run it.
    uint8_t command(uint8_t cmd, uint8_t drive, std::vector<uint8_t> args = {}, uint32_t data = 0,
                    uint16_t at = IOPB) {
        poke((uint16_t)(at + CMD), cmd);
        poke((uint16_t)(at + DRV), drive);
        for (int i = 0; i < 7; ++i)
            poke((uint16_t)(at + ARG1 + i), i < (int)args.size() ? args[(size_t)i] : 0);
        pokeWord((uint16_t)(at + DATA), (uint16_t)data);
        poke((uint16_t)(at + DATA + 2), (uint8_t)(data >> 16));
        return attn(at);
    }

    // R/W in GLOBAL mode 0: sector, logical track, count.
    uint8_t rw(bool read, int drive, int sector, int track, int count, uint32_t data = BUF) {
        return command(RW, (uint8_t)drive,
                       {(uint8_t)(read ? 1 : 0), (uint8_t)sector, (uint8_t)(sector >> 8), (uint8_t)track,
                        (uint8_t)(track >> 8), (uint8_t)count, (uint8_t)(count >> 8)},
                       data);
    }

    // The SPECIFY block for this drive with `reserved` tracks (p. 18-19).
    void specifyBlock(uint16_t at, int reserved) {
        for (int i = 0; i < 22; ++i) poke((uint16_t)(at + i), 0);
        pokeWord((uint16_t)(at + 4), kSize);
        pokeWord((uint16_t)(at + 6), kSec);
        pokeWord((uint16_t)(at + 8), kHeads);
        pokeWord((uint16_t)(at + 10), kCyl);
        pokeWord((uint16_t)(at + 18), (uint16_t)reserved);
    }

    bool sectorAt(uint16_t a, int physTrack, int sec) {
        const uint64_t off = offsetOf(physTrack, sec);
        for (int i = 0; i < kSize; ++i)
            if (mem((uint16_t)(a + i)) != stamp(off + (uint64_t)i)) return false;
        return true;
    }

    bool hasLog(const std::string& part) {
        for (const auto& s : hd->drainLog())
            if (s.find(part) != std::string::npos) return true;
        return false;
    }

    // A mounted drive 0, the board reset and warmed up, GLOBAL mode 0.
    void ready(bool ro = false) {
        std::string err;
        hd->mountWith("drive0", "hd.dsk", ro, kShape, err);
        resetAndWarmUp();
        command(GLOBAL, 0, {0x00, 10, 4});
    }
};

bool decodes(CompuProDisk3Board* hd, Cycle type, uint8_t port) {
    BusCycle c;
    c.type = type;
    c.addr = port;
    return hd->decodes(c);
}

} // namespace

void test_disk3() {
    SECTION("disk3: the port pair");
    {
        Rig k;
        CHECK(decodes(k.hd, Cycle::IoWrite, 0x90) && decodes(k.hd, Cycle::IoWrite, 0x91),
              "90 and 91 are the attention port");
        CHECK(!decodes(k.hd, Cycle::IoRead, 0x90), "the port is write-only: an IN drives nothing");
        CHECK(!decodes(k.hd, Cycle::IoWrite, 0x92) && !decodes(k.hd, Cycle::IoWrite, 0x8F),
              "a pair and no more");
        std::string err;
        CHECK(!setProperty(*k.hd, "port", "91", err), "an odd base is refused: S1 sets A7-A1");
    }

    SECTION("disk3: mounting");
    {
        std::string err;
        withDisk(1000);
        Rig k;
        CHECK(!k.hd->mount("drive0", "x.dsk", false, err), "a size that is not the drive's is refused");
        CHECK(err.find("37748736") != std::string::npos, "...and the error gives the Q540 size");
        withDisk(512ull * 8 * 9 * 1024);
        CHECK(k.hd->mount("drive0", "q540.dsk", false, err), "a Q540 image (512 x 8 x 9 x 1024) mounts");
        withDisk(kBytes);
        CHECK(!k.hd->mount("drive1", "small.dsk", false, err), "a small image is not a Q540");
        CHECK(k.hd->mountWith("drive1", "small.dsk", false, kShape, err), "...with its shape it mounts");
        withDisk(0);
        CHECK(k.hd->mountWith("drive2", "new.dsk", false, kShape, err) && k.hasLog("new disk, 73728 bytes of E5"),
              "an empty file is a new disk, filled with E5");
    }

    SECTION("disk3: the first attention only fetches the LINK");
    {
        withDisk(kBytes);
        Rig k;
        k.poke(IOPB + CMD, NOOP);
        k.poke(IOPB + STAT, 0);
        k.link(0x50, 0x012345);  // 24 bits: the board keeps all three bytes
        k.resetAndWarmUp();
        CHECK(k.hd->link() == 0x012345 && !k.hd->busy(), "OUT 01, OUT 00: the LINK at 5D-5F is kept");
        CHECK(k.mem(IOPB + STAT) == 0, "...and nothing runs, no status (p. 11)");
        k.link(0x50, IOPB);
        k.resetAndWarmUp();
        CHECK(k.command(NOOP, 0) == 0xFF, "the next attention runs the NOOP at the LINK: STATUS FF");
        CHECK(k.command(VERSION, 0) == 0xFF && k.mem(IOPB + ARG1) == 0x0D && k.hasLog("not in the manual"),
              "VERSION answers 0D in ARG1, and says the number is chosen");
        k.runCode({0x3E, 0x02, 0xD3, PORT, 0x76});
        CHECK(!k.hd->held(), "OUT 02 is another board's reset bit (J10): not ours");
        k.runCode({0x3E, 0xFF, 0xD3, PORT, 0x76});
        CHECK(k.hd->held(), "OUT FF (the manual's reset) holds ours too");
    }

    SECTION("disk3: NOT READY and an argument range error");
    {
        withDisk(kBytes);
        Rig k;
        k.ready();
        CHECK(k.command(HOME, 1) == 2, "HOME on drive 1, no disk: STATUS 02, not ready (D3INIT's test)");
        CHECK(k.command(HOME, 7) == 1, "drive 7: STATUS 01, argument range");
        CHECK(k.command(0x3F, 0) == 1, "an unknown operation code: STATUS 01");
        CHECK(k.command(EXAMINE, 0) == 1 && k.hasLog("not modelled"), "EXAMINE is not modelled, and says so");
        CHECK(k.command(HOME, 0) == 0xFF && k.hd->cylinder(0) == 0, "HOME on drive 0: FF");
        CHECK(k.command(SEEK, 0, {3, 0}) == 0xFF && k.hd->cylinder(0) == 3, "SEEK to cylinder 3");
        CHECK(k.command(SEEK, 0, {4, 0}) == 3 && k.hd->cylinder(0) == 3, "SEEK past the last cylinder: TIME OUT");
    }

    SECTION("disk3: R/W, one sector, as the BIOS reads it");
    {
        withDisk(kBytes);
        Rig k;
        k.ready();
        CHECK(k.rw(true, 0, 4, 3, 1) == 0xFF, "read logical track 3 sector 4: FF");
        CHECK(k.sectorAt(BUF, 3, 4), "...the 1024 bytes of cylinder 1 head 1 sector 4 are at 2000");
        CHECK(k.word(IOPB + ARG1 + 1) == 4 && k.word(IOPB + ARG1 + 3) == 3 && k.word(IOPB + ARG1 + 5) == 0,
              "...the IOPB names the last sector moved, count 0");
        CHECK(k.addr3(IOPB + DATA) == BUF, "...and DATA is the first byte of the last sector");
        CHECK(k.hd->cylinder(0) == 1, "the heads are on cylinder 1");
    }

    SECTION("disk3: R/W, three sectors across a track");
    {
        withDisk(kBytes);
        Rig k;
        k.ready();
        CHECK(k.rw(true, 0, 8, 0, 3) == 0xFF, "read 3 from track 0 sector 8");
        CHECK(k.sectorAt(BUF, 0, 8) && k.sectorAt(BUF + 1024, 1, 0) && k.sectorAt(BUF + 2048, 1, 1),
              "...0/8, 1/0, 1/1 in a row");
        CHECK(k.word(IOPB + ARG1 + 1) == 1 && k.word(IOPB + ARG1 + 3) == 1, "...the IOPB ends at track 1 sector 1");
        CHECK(k.addr3(IOPB + DATA) == BUF + 2048, "...DATA at the last sector, 2800");
    }

    SECTION("disk3: absolute sectors, the mode after a reset");
    {
        withDisk(kBytes);
        Rig k;
        std::string err;
        k.hd->mountWith("drive0", "hd.dsk", false, kShape, err);
        k.resetAndWarmUp();
        CHECK(k.command(RW, 0, {1, 10, 0, 0, 0, 1, 0}, BUF) == 0xFF, "no GLOBAL yet: ARG2-5 is absolute sector 10");
        CHECK(k.sectorAt(BUF, 1, 1), "...which is track 1 sector 1");
        CHECK(k.word(IOPB + ARG1 + 1) == 10 && k.word(IOPB + ARG1 + 3) == 0, "...and the IOPB keeps the absolute form");
    }

    SECTION("disk3: the reset bit restores the firmware's defaults");
    {
        // p. 17-18: "Reset the board by pulsing the reset bit in the attention port" ... "the
        // DISK 3 firmware defaults to ST506 parameters and absolute sector mode after a reset".
        // A boot EPROM leaves GLOBAL mode 0 and a SPECIFY behind; the OS's own reset must undo
        // them, or its read of absolute sector 0 (the label) lands on logical track 0.
        withDisk(kBytes);
        Rig k;
        k.ready();
        k.specifyBlock(PARM, 3);
        CHECK(k.command(SPECIFY, 0, {}, PARM) == 0xFF, "GLOBAL mode 0, then SPECIFY 3 reserved tracks");
        CHECK(k.rw(true, 0, 0, 0, 1) == 0xFF && k.sectorAt(BUF, 4, 0), "...sector 0 track 0 is physical track 4");
        k.resetAndWarmUp();
        CHECK(k.command(RW, 0, {1, 0, 0, 0, 0, 1, 0}, BUF) == 0xFF && k.sectorAt(BUF, 0, 0),
              "after the reset bit: ARG2-5 is absolute sector 0 again, physical track 0");
        CHECK(k.command(RW, 0, {1, 10, 0, 0, 0, 1, 0}, BUF) == 0xFF && k.sectorAt(BUF, 1, 1),
              "...absolute sector 10 is track 1 sector 1: no reserved tracks either");
    }

    SECTION("disk3: WRITE, then read it back; write-protect");
    {
        withDisk(kBytes);
        Rig k;
        k.ready();
        for (int i = 0; i < 2048; ++i) k.poke((uint16_t)(BUF + i), (uint8_t)(i * 3));
        CHECK(k.rw(false, 0, 2, 5, 2) == 0xFF, "write 2 sectors at track 5 sector 2");
        for (int i = 0; i < 2048; ++i) k.poke((uint16_t)(0x4000 + i), 0);
        CHECK(k.rw(true, 0, 2, 5, 2, 0x4000) == 0xFF, "read them back to 4000");
        bool same = true;
        for (int i = 0; i < 2048; ++i) same = same && k.mem((uint16_t)(0x4000 + i)) == (uint8_t)(i * 3);
        CHECK(same, "...the same 2048 bytes");
        Rig ro;
        ro.ready(true);
        CHECK(ro.rw(false, 0, 0, 0, 1) == 5 && ro.hasLog("write-protected"), "a read-only disk: STATUS 05, write fault");
    }

    SECTION("disk3: TIME OUT leaves the IOPB to retry");
    {
        withDisk(kBytes);
        Rig k;
        k.ready();
        // Logical tracks 0-7 exist (4 cylinders x 2 heads). Start at track 7 sector 7, four sectors.
        CHECK(k.rw(true, 0, 7, 7, 4) == 3 && k.hasLog("no header for logical track 8"),
              "a read that runs off the drive: STATUS 03");
        CHECK(k.word(IOPB + ARG1 + 1) == 0 && k.word(IOPB + ARG1 + 3) == 8 && k.word(IOPB + ARG1 + 5) == 2,
              "...the IOPB names the failing sector (8/0) with 2 left");
        CHECK(k.addr3(IOPB + DATA) == BUF + 2048, "...and DATA is the next byte to use");
        CHECK(k.sectorAt(BUF, 7, 7) && k.sectorAt(BUF + 1024, 7, 8), "the two good sectors moved");
    }

    SECTION("disk3: SPECIFY with reserved tracks, and SET-MAP");
    {
        withDisk(kBytes);
        Rig k;
        k.ready();
        k.specifyBlock(PARM, 3);
        CHECK(k.command(SPECIFY, 0, {}, PARM) == 0xFF, "SPECIFY: 3 reserved tracks");
        CHECK(k.rw(true, 0, 0, 0, 1) == 0xFF && k.sectorAt(BUF, 4, 0),
              "logical track 0 is physical track 4: the label track, then the 3 reserved");
        for (int i = 0; i < 256; ++i) k.poke((uint16_t)(PARM + 0x100 + i), 0);
        k.pokeWord(PARM + 0x100 + 2, 4);       // entry 1: logical track 4 -> physical track 2
        k.pokeWord(PARM + 0x100 + 4, 0xFFFF);  // the end
        CHECK(k.command(SETMAP, 0, {}, PARM + 0x100) == 0xFF, "SET-MAP: track 4 relocated by entry 1");
        CHECK(k.rw(true, 0, 5, 4, 1) == 0xFF && k.sectorAt(BUF, 2, 5), "logical track 4 reads physical track 2");
        CHECK(k.rw(true, 0, 5, 3, 1) == 0xFF && k.sectorAt(BUF, 7, 5), "logical track 3 is not in the map: 3 + 1 + 3");
        k.specifyBlock(PARM, 0);
        k.pokeWord(PARM + 6, 8);
        CHECK(k.command(SPECIFY, 0, {}, PARM) == 0xFF && k.hasLog("sectors outside the image will time out"),
              "a SPECIFY that does not fit the image is taken, and said");
        CHECK(k.rw(true, 0, 0, 0, 1) == 3, "...and its sectors time out");
    }

    SECTION("disk3: FORMAT a track");
    {
        withDisk(kBytes);
        Rig k;
        k.ready();
        CHECK(k.command(SEEK, 0, {2, 0}) == 0xFF, "SEEK to cylinder 2");
        // The header image: (cylinder, cylinder, head, sector), interleave 2.
        const int order[kSec] = {0, 5, 1, 6, 2, 7, 3, 8, 4};
        for (int i = 0; i < kSec; ++i) {
            k.poke((uint16_t)(PARM + i * 4 + 0), 0);
            k.poke((uint16_t)(PARM + i * 4 + 1), 2);
            k.poke((uint16_t)(PARM + i * 4 + 2), 1);
            k.poke((uint16_t)(PARM + i * 4 + 3), (uint8_t)order[i]);
        }
        CHECK(k.command(FORMAT, 0, {0x10, 0xAA, 1}, PARM) == 0xFF, "FORMAT head 1, fill AA");
        CHECK(k.rw(true, 0, 6, 5, 1) == 0xFF && k.mem(BUF) == 0xAA && k.mem(BUF + 1023) == 0xAA,
              "cylinder 2 head 1 (logical track 5) reads AA");
        CHECK(k.rw(true, 0, 0, 4, 1) == 0xFF && k.sectorAt(BUF, 4, 0), "head 0 of cylinder 2 is as it was");
    }

    SECTION("disk3: chained IOPBs and the interrupt");
    {
        withDisk(kBytes);
        Rig k;
        k.ready();
        std::string err;
        setProperty(*k.hd, "interrupt", "vi1", err);
        // IOPB: HOME with continue -> IOPB2: NOOP with interrupt.
        k.poke(IOPB + CMD, HOME | 0x40);
        k.poke(IOPB + DRV, 0);
        k.link(IOPB, IOPB2);
        k.poke(IOPB2 + CMD, NOOP | 0x80);
        k.poke(IOPB2 + STAT, 0);
        k.link(IOPB2, IOPB);
        k.attn(IOPB2);
        CHECK(k.mem(IOPB + STAT) == 0xFF && k.mem(IOPB2 + STAT) == 0xFF, "one attention runs both");
        CHECK(k.hd->assertsVi() == 0x02, "the last asked for the interrupt: VI1");
        k.poke(IOPB + CMD, NOOP);
        k.attn(IOPB);
        CHECK(k.hd->assertsVi() == 0, "the next attention clears it; NOOP without bit 7 asks for none");
    }

    SECTION("disk3: SNAPSHOT / RESTORE");
    {
        withDisk(kBytes);
        Rig k;
        k.ready();
        k.specifyBlock(PARM, 2);
        k.command(SPECIFY, 0, {}, PARM);
        k.command(SEEK, 0, {3, 0});
        StateWriter w;
        k.hd->serialize(w);
        Rig k2;
        std::string err;
        k2.hd->mountWith("drive0", "hd.dsk", false, kShape, err);
        StateReader r(w.data());
        k2.hd->deserialize(r);
        CHECK(r.ok() && k2.hd->cylinder(0) == 3 && k2.hd->link() == IOPB, "the heads and the LINK come back");
        CHECK(k2.rw(true, 0, 1, 0, 1) == 0xFF && k2.sectorAt(BUF, 3, 1),
              "...with GLOBAL mode 0 and the SPECIFY: logical track 0 is physical track 3");
    }
}
