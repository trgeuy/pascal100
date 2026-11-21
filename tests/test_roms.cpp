#include "test.h"

#include "core/crc32.h"
#include "core/roms.h"

using namespace altair;

// DESIGN.md 0.1 applied to binaries. A ROM image is a HARDWARE FACT: if a bit is
// wrong, every piece of software above it gets debugged against the wrong ground
// truth, and it looks like a software bug for a very long time. So the CRC of
// every built-in is checked here, against the value recorded in docs/roms.md --
// and someone's editor mangling a binary becomes a failing test instead of a
// mystery.
void test_roms() {
    SECTION("built-in ROMs -- provenance (docs/roms.md)");

    CHECK(!builtinRoms().empty(), "at least one ROM is compiled in");

    // pascal100 (2026-11-09): the Altair ROMs (DBL, MDBL, CDBL, the monitors, ...) are not
    // built in. DBL and ALTMON are kept as test fixtures in tests/media/ (README there).
    for (const char* gone : {"dbl", "mdbl", "cdbl", "altmon", "amon", "mbl"})
        CHECK(findRom(gone) == nullptr, (std::string("builtin:") + gone + " is not built in").c_str());

    // Every built-in gets its CRC32 checked against the value recorded in docs/roms.md. A
    // corrupted embed then fails the BUILD, not a user chasing a phantom software bug.
    // Provenance and the per-ROM listing each was verified against are in docs/roms.md and
    // roms/<NAME>/.
    struct Case {
        const char* name;
        uint16_t lo, hi;
        size_t size;
        uint32_t crc;
        bool contiguous;
    };
    const Case cases[] = {
        // The Tarbell floppy boot PROM: 32 bytes at 0000, shared by the single- and
        // double-density cards. Reads track 0 sector 1 into 0000 and jumps into it (07D).
        {"tarbell-sd", 0x0000, 0x001F, 32, 0x55E3446Du, true},
        // The Jade Double D host boot PROM (DDBOOT, CP/M 2.2 Release 2): 590 bytes at F000,
        // assembled by DRI ASM from Jade's own source on the master disk. It starts the board
        // Z80 and is the console for Jade's BIOS, so it stays mapped while CP/M runs. An ORG
        // puts the injection module's NMI handler at its offset 0066h, which leaves F1E3-F1EB
        // unprogrammed (read as FF).
        {"ddboot",           0xF000, 0xF24D,  590, 0x631C3AD5u, false},
        // The same PROM patched at Jade's patch points for an 88-2SIO console at 10/11 (INIT
        // resets the 6850; status bits RDRF/TDRE). Every address, the gap included, is unmoved.
        {"ddboot-2sio",      0xF000, 0xF24D,  590, 0xA017E174u, false},
        // SSM Microcomputer Products 8080 System Monitor V1.0 (C. E. Ohme): a single 2K
        // EPROM at F000-F7FF assembled from three independent modules -- the monitor (F000),
        // the System Configuration Package/driver tables (F600), and the VB1 video driver
        // (F700). They do not link against one another, so the image has two small gaps that
        // read FF (F5EE-F5FF and F6EA-F6FF) -> non-contiguous, CRC over the FF-filled 2K span.
        // Built with M80/L80 under CP/M (roms/SSM-8080MON/, docs/devguide/assembling-roms.md);
        // boots to "MONITOR V1.0" with a working prompt. This is the PB1 manual's MONIT.
        {"ssm-8080mon", 0xF000, 0xF7FF, 2048, 0x27636BB0u, false},
        // The CompuPro Disk 1A's 2764: sixteen 512-byte boot routines in one full 8 KB part.
        // The board shows one routine at 0000 (`routine`), so the image starts at 0 too.
        {"disk1a", 0x0000, 0x1FFF, 8192, 0x93DBAEF4u, true},
        // The CompuPro PC Video's 2764 character generator, in CompuPro's layout with the
        // Modern DOS glyphs (CC0): 9x14 monochrome at 0000, 8x8 color at 1000.
        {"pcvideo", 0x0000, 0x1FFF, 8192, 0x6555EFCDu, true},
        // The CompuPro Disk 1's 2716, a factory dump: eight 256-byte routines, 0-3 for the 8086
        // and 4-7 for the 8080/Z80. The board shows one at 0000 (`routine`).
        {"disk1", 0x0000, 0x07FF, 2048, 0xEBA8EE41u, true},
    };
    for (const auto& c : cases) {
        std::string tag = std::string("builtin:") + c.name;
        const BuiltinRom* r = findRom(c.name);
        CHECK(r != nullptr, (tag + " exists").c_str());
        if (!r) continue;

        Image ri;
        std::string rerr;
        CHECK(decodeRom(*r, 0, ri, rerr), (tag + " decodes (every record checksums)").c_str());
        CHECK(ri.lo() == c.lo && ri.hi() == c.hi,
              (tag + " occupies the range docs/roms.md records").c_str());
        CHECK(ri.flat().size() == c.size, (tag + " decodes to its recorded size").c_str());
        CHECK(ri.contiguous() == c.contiguous,
              (tag + " gap structure matches docs/roms.md").c_str());
        CHECK(crc32(ri.flat()) == c.crc, (tag + " CRC32 matches docs/roms.md").c_str());
    }
}
