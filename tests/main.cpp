#include "test.h"

#include "boards/mits-2sio.h"
#include "boards/mits-88c700.h"
#include "boards/gsio.h"
#include "boards/io4.h"
#include "boards/compupro-interfacer1.h"
#include "boards/compupro-interfacer3.h"
#include "boards/compupro-interfacer4.h"
#include "boards/compupro-disk1.h"
#include "boards/compupro-ss1.h"
#include "boards/terminal-font.h"
#include "host/display_null.h"
#include "host/endpoint.h"
#include "host/cardimg.h"
#include "host/media.h"
#include "host/terminal/stream.h"

#include <cstdio>

int g_fail = 0;
int g_run = 0;

namespace {

// The whole suite as an ordered name->function table, keyed by each test
// function's name minus its `test_` prefix. No args runs all of it in this
// order (identical to the historic flat call list, so the `unit` ctest test and
// CI are unaffected); named args run only the matching rows. See the plan and
// CLAUDE.md's Testing section -- this is a local iteration speed-up, not a
// substitute for the pre-commit full run.
const struct {
    const char* name;
    void (*fn)();
} kTests[] = {
    {"paths", test_paths},
    {"hex", test_hex},
    {"srec", test_srec},
    {"symbols", test_symbols},
    {"media", test_media},
    {"tnfs", test_tnfs},
    {"cardimg", test_cardimg},
    {"imd", test_imd},
    {"roms", test_roms},
    {"clock", test_clock},
    {"busclock", test_busclock},
    {"statefile", test_statefile},
    {"snapshot", test_snapshot},
    {"bus", test_bus},
    {"ieee696", test_ieee696},
    {"memory", test_memory},
    {"memory_segments", test_memory_segments},
    {"bankmem", test_bankmem},
    {"readonly_props", test_readonly_props},
    {"save_is_a_read", test_save_is_a_read},
    {"phantom", test_phantom},
    {"cli", test_cli},
    {"console", test_console},
    {"debuglog", test_debuglog},
    {"lineedit", test_lineedit},
    {"idle_judgement", test_idle_judgement},
    {"should_pace", test_should_pace},
    {"may_nap", test_may_nap},
    {"achieved_hz", test_achieved_hz},
    {"boundary", test_boundary},
    {"numbers", test_numbers},
    {"units", test_units},
    {"machines", test_machines},
    {"load_is_atomic", test_load_is_atomic},
    {"clock_survives_load", test_clock_survives_load},
    {"subunit_schema", test_subunit_schema},
    {"toml_notes", test_toml_notes},
    {"toml_errors", test_toml_errors},
    {"isa", test_isa},
    {"z80_isa", test_z80_isa},
    {"8085_isa", test_8085_isa},
    {"8088_isa", test_8088_isa},
    {"cpu", test_cpu},
    {"status", test_status},
    {"z80_cpu", test_z80_cpu},
    {"8085_cpu", test_8085_cpu},
    {"8085_undoc_alu", test_8085_undoc_alu},
    {"expr", test_expr},
    {"debug", test_debug},
    {"coreswitch", test_coreswitch},
    {"cpu8588", test_cpu8588},
    {"pascal100", test_pascal100},
    {"ddt", test_ddt},
    {"dma", test_dma},
    {"sio2", test_sio2},
    {"gsio", test_gsio},
    {"io4", test_io4},
    {"interfacer1", test_interfacer1},
    {"interfacer3", test_interfacer3},
    {"interfacer4", test_interfacer4},
    {"boardlog", test_boardlog},
    {"pmachine", test_pmachine},
    {"lines", test_lines},
    {"modemline", test_modemline},
    {"telnet", test_telnet},
    {"wd17xx", test_wd17xx},
    {"upd765", test_upd765},
    {"bitserial", test_bitserial},
    {"i8257", test_i8257},
    {"am9511a", test_am9511a},
    {"am9512", test_am9512},
    {"tarbell", test_tarbell},
    {"jadedd", test_jadedd},
    {"cpu8088", test_cpu8088},
    {"disk1", test_disk1},
    {"disk1a", test_disk1a},
    {"disk2", test_disk2},
    {"spindle", test_spindle},
    {"c700", test_c700},
    {"pb1", test_pb1},
    {"printer", test_printer},
    {"tee", test_tee},
    {"mirror", test_mirror},
    {"pty", test_ptystream},
    {"terminal", test_terminal},
    {"multiwindow", test_multiwindow},
    {"framedump", test_framedump},
    {"ss1", test_ss1},
    {"rtc100", test_rtc100},
    {"hostdir", test_hostdir},
    {"hostbridge", test_hostbridge},
    {"mcp", test_mcp},
};

bool nameEq(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b) {
        unsigned char ca = static_cast<unsigned char>(*a);
        unsigned char cb = static_cast<unsigned char>(*b);
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return false;
    }
    return *a == *b;
}

} // namespace

int main(int argc, char** argv) {
    // THE SAME WIRING main() DOES (DESIGN.md 7.7). The monitor knows the endpoint
    // grammar; boards do not. If the tests installed a DIFFERENT resolver -- a
    // convenient one that quietly turned `console` into a NullStream, say -- then
    // machines/default.toml would be exercised here in a configuration that no
    // user will ever run, and the first thing to break would be the real one.
    altair::Sio2Board::setResolver(altair::resolveEndpoint);
    altair::C700Board::setResolver(altair::resolveEndpoint);
    altair::StrapSerialBoard::setResolver(altair::resolveEndpoint);
    altair::InterfacerOneBoard::setResolver(altair::resolveEndpoint);
    altair::InterfacerThreeBoard::setResolver(altair::resolveEndpoint);
    altair::InterfacerFourBoard::setResolver(altair::resolveEndpoint);
    altair::Io4Board::setResolver(altair::resolveEndpoint);
    altair::CompuProDisk1Board::setResolver(altair::resolveEndpoint);
    altair::Ss1Board::setResolver(altair::resolveEndpoint);

    // A graphics board draws into an injected Display; headless tests give it a
    // NullDisplay, so a VDM-1 renders into memory and a test reads the pixels back
    // with no window. The SAME injection main() does, one backend down.
    static altair::NullDisplay g_display;

    // The generic terminal endpoint reads these the same way (issue #244). A NullDisplay is
    // not windowed, so `terminal:` refuses at CONNECT here -- which test_terminal asserts,
    // while it drives the VT100 engine directly (no window, no endpoint) to read the grid.
    altair::TerminalStream::setDisplay(&g_display);
    altair::TerminalStream::setFont(&altair::bundledTerminalFont());

    // The REAL media resolver, for the same reason -- and the SAME one the CLI
    // installs: openHostMedia routes an image with a `.geo` sidecar to a CardImage and a
    // plain file to a HostFile. A test that wants a disk without a filesystem installs a
    // MemoryMedia resolver for its length and puts this one back -- see test_media.cpp.
    altair::setMediaResolver(altair::openHostMedia);

    const int kCount = static_cast<int>(sizeof(kTests) / sizeof(kTests[0]));

    // Collect selectors, and handle --list up front.
    bool haveSelectors = false;
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (nameEq(arg, "--list") || nameEq(arg, "-l")) {
            for (int t = 0; t < kCount; ++t)
                std::printf("%s\n", kTests[t].name);
            return 0;
        }
        haveSelectors = true;
    }

    if (!haveSelectors) {
        // No args: run the whole table in order -- identical to the historic behavior.
        for (int t = 0; t < kCount; ++t)
            kTests[t].fn();
    } else {
        // Named selectors: an unknown name is a hard error, so a typo can't run
        // zero tests and print "0 failed" (which would read as a pass).
        for (int i = 1; i < argc; ++i) {
            bool matched = false;
            for (int t = 0; t < kCount; ++t) {
                if (nameEq(argv[i], kTests[t].name)) { matched = true; break; }
            }
            if (!matched) {
                std::fprintf(stderr,
                    "error: unknown test '%s' (try --list)\n", argv[i]);
                return 2;
            }
        }
        // Run matching rows in table order, each under a header.
        for (int t = 0; t < kCount; ++t) {
            bool selected = false;
            for (int i = 1; i < argc; ++i) {
                if (nameEq(argv[i], kTests[t].name)) { selected = true; break; }
            }
            if (!selected) continue;
            std::printf("== %s ==\n", kTests[t].name);
            kTests[t].fn();
        }
    }

    std::printf("\n%d checks, %d failed\n", g_run, g_fail);
    return g_fail ? 1 : 0;
}
