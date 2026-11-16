#pragma once
#include <cstdio>
#include <string>

// The absolute path of a fixture under tests/media/ -- e.g. testRom("roms/DBL.HEX"). pascal100
// (2026-11-09): the ROMs upstream's tests mounted as `builtin:dbl` / `builtin:altmon` are test
// fixtures now, mounted by path (tests/media/roms/README.md).
#ifdef ALTAIR_SOURCE_DIR
inline std::string testRom(const char* rel) {
    return std::string(ALTAIR_SOURCE_DIR) + "/tests/media/" + rel;
}
#endif

extern int g_fail;
extern int g_run;

#define CHECK(cond, what)                                                                \
    do {                                                                                 \
        ++g_run;                                                                         \
        if (!(cond)) {                                                                   \
            ++g_fail;                                                                    \
            std::printf("  FAIL  %s\n        at %s:%d\n", (what), __FILE__, __LINE__);   \
        }                                                                                \
    } while (0)

#define SECTION(name) std::printf("\n%s\n", name)

void test_clock();
void test_busclock();
void test_statefile();
void test_snapshot();
void test_bus();
void test_ieee696();
void test_memory();
void test_memory_segments();
void test_readonly_props();
void test_save_is_a_read();
void test_paths();
void test_hex();
void test_srec();
void test_symbols();
void test_media();
void test_tnfs();
void test_cardimg();
void test_imd();
void test_roms();
void test_phantom();
void test_cli();
void test_console();
void test_debuglog();
void test_lineedit();
void test_idle_judgement();
void test_should_pace();
void test_may_nap();
void test_achieved_hz();
void test_boundary();
void test_numbers();
void test_units();
void test_machines();
void test_load_is_atomic();
void test_clock_survives_load();
void test_subunit_schema();
void test_toml_notes();
void test_toml_errors();
void test_isa();
void test_z80_isa();
void test_pmachine();
void test_8085_isa();
void test_8088_isa();
void test_cpu();
void test_status();
void test_z80_cpu();
void test_8085_cpu();
void test_8085_undoc_alu();
void test_expr();
void test_debug();
void test_coreswitch();
void test_cpu8588();
void test_cpu8687();
void test_cpu286();
void test_pascal100();
void test_ddt();
void test_dma();
void test_sio2();
void test_io4();
void test_interfacer1();
void test_interfacer3();
void test_interfacer4();
void test_boardlog();
void test_lines();
void test_modemline();
void test_telnet();
void test_wd17xx();
void test_upd765();
void test_floppy_drive();
void test_bitserial();
void test_i8257();
void test_scn2681();
void test_mc6845();
void test_am9511a();
void test_am9512();
void test_tarbell();
void test_jadedd();
void test_spuz();
void test_pcvideo();
void test_cpu8088();
void test_cpu80286();
void test_disk1();
void test_disk1a();
void test_disk2();
void test_disk3();
void test_mdriveh();
void test_spindle();
void test_printer();
void test_tee();
void test_mirror();
void test_ptystream();
void test_terminal();
void test_multiwindow();
void test_framedump();
void test_ss1();
void test_hostdir();
void test_hostbridge();
void test_mcp();

// The Developer Guide's worked example (tests/boards/lamp/). Not a shipping
// board -- it is compiled into the test binary only, so the tutorial's code
// cannot rot, and the reader still performs the registry step themselves.
