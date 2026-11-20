cmake_minimum_required(VERSION 3.20)
project(pascal100 VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# WERROR turns those warnings into build failures. OFF by default -- a casual build, or an
# unfamiliar toolchain whose newer diagnostics we have not yet chased down, must still compile.
# CI passes -DWERROR=on on every leg (.github/workflows/ci.yml), so a warning fails the build
# before merge; run `cmake -B build -DWERROR=on` locally to reproduce that gate before a PR.
#
# All three toolchains, now. GCC and Clang are clean at -Wall -Wextra -Wpedantic -Wshadow;
# MSVC is clean at /W4 once two things are handled (issue #238):
#
#   * Variable shadowing (MSVC C4456/C4457) is FIXED at the source -- the shadowing locals
#     were renamed. -Wshadow on GCC/Clang keeps the class from creeping back on every CI leg,
#     not just on Windows. The two do not flag the same set: GCC's -Wshadow reports a local
#     declared inside a lambda that hides a local of the enclosing function even when the
#     lambda does not capture it; Clang keeps that case in -Wshadow-uncaptured-local. Without
#     that flag a Mac build passes code that reds the Linux leg (issue #548), so Clang gets it
#     too. GCC rejects the flag as unknown, hence Clang only.
#   * Integer narrowing (MSVC C4244/C4267 -- uint32->uint8/16, size_t->uint32) is SUPPRESSED
#     tree-wide with /wd4244 /wd4267. It is the daily bread of an 8-bit emulator's byte/word
#     masking; GCC/Clang do not flag it at -Wall -Wextra either (only -Wconversion would), so
#     casting every truncation would be thousands of noise static_casts for no bug caught.
#
# With those settled, /WX joins the gate under WERROR on MSVC, matching -Werror on the others.
option(WERROR "Treat compiler warnings as errors; CI enforces this on all three toolchains." OFF)
if(MSVC)
  add_compile_options(/W4 /permissive- /wd4244 /wd4267)
  # The CRT "secure" deprecations (C4996 on getenv/fopen/strcpy/...) are MSVC nagging us to
  # use its non-portable _s variants. We use the standard C functions on purpose -- the code
  # is cross-platform -- so turn that family off. This is narrower than /wd4996: a genuine
  # [[deprecated]] use still warns.
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
  if(WERROR)
    add_compile_options(/WX)
  endif()
  # AN 8 MB STACK, AS ON LINUX AND macOS. The Windows default is 1 MB, an eighth of theirs.
  # MSVC gives Monitor::exec a frame of about 49 KB (it does not let the locals of separate
  # blocks share stack space; Clang needs about 20 KB), and DO nests exec. Twice a test that was
  # green on the other platforms overflowed here: 4644401f cut the DO depth cap from 32 to 10 to
  # fit, and #780 then went over with a few new locals. The value is the other platforms', not
  # a number tuned to one frame. It is address space only: pages are committed as they are used,
  # and a std::thread takes the same reserve.
  add_link_options(/STACK:8388608)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wshadow-uncaptured-local)
  endif()
  if(WERROR)
    add_compile_options(-Werror)
  endif()
endif()

# ---------------------------------------------------------------------------
# SANITIZE -- AddressSanitizer + UndefinedBehaviorSanitizer, in one flag.
#
# For memory and undefined-behaviour bugs that one platform's allocator hides and another's
# does not -- a use-after-free that is harmless on macOS can crash only on Windows. No
# compiler warning fires for those; a sanitizer names the exact file:line. Configure a side
# tree with it and the whole binary (library, tools, tests) is instrumented:
#
#   cmake -S . -B build-asan -DSANITIZE=on
#   cmake --build build-asan --target altair_tests
#   ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 ./build-asan/altair_tests
#
# Keep it in its OWN build directory -- an instrumented binary is slower and is not
# what you ship. It is deliberately NOT on by default and NOT wired into CI: CI catches
# a platform-only crash by running on that platform, and this is the tool you reach for
# once it does.
#
# GCC/Clang get both sanitizers. MSVC has had /fsanitize=address since VS2019 and it
# catches the same use-after-free class, so honour the flag there too -- it just has no
# UBSan half to add. (MSVC ASan wants the RELEASE CRT and its runtime DLL on PATH; a
# Debug config or a stale PATH is the usual "it built but won't launch".)
# ---------------------------------------------------------------------------
option(SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer (dev only)" OFF)
if(SANITIZE)
  if(MSVC)
    add_compile_options(/fsanitize=address)
    message(STATUS "SANITIZE=on -- AddressSanitizer (MSVC has no UBSan). "
                   "Use a Release config and keep the ASan runtime DLL on PATH.")
  else()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
    message(STATUS "SANITIZE=on -- Address + UndefinedBehavior sanitizers active. "
                   "Run with ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1.")
  endif()
endif()

# ---------------------------------------------------------------------------
# Built-in ROMs (DESIGN.md 10.3.1)
#
# Each subdirectory of roms/ is one ROM. Its .HEX or .BIN is embedded VERBATIM
# as a byte array; the C++ side runs the ordinary Intel HEX loader over those
# bytes at startup. One parser, two sources (a host file, or .rodata) -- so a
# built-in cannot drift from a mounted file, and CMake never learns what Intel
# HEX is. Provenance and CRC32 for every ROM live in docs/roms.md.
# ---------------------------------------------------------------------------
# CONFIGURE_DEPENDS: re-glob on every BUILD, not just on configure. Without it,
# dropping a new ROM into roms/ does nothing until someone remembers to re-run
# cmake -- and DELETING one breaks the build outright, with an error naming a
# file you just deliberately removed. Both are ten confusing minutes, and both
# are avoidable for one word.
set(ROMS_GENERATED "${CMAKE_BINARY_DIR}/generated/roms_generated.cpp")
file(GLOB ROM_DIRS RELATIVE "${CMAKE_SOURCE_DIR}/roms"
     CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/roms/*")

set(ROM_IMAGE_FILES "")
foreach(d ${ROM_DIRS})
  if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/roms/${d}")
    file(GLOB img CONFIGURE_DEPENDS
                  "${CMAKE_SOURCE_DIR}/roms/${d}/*.HEX" "${CMAKE_SOURCE_DIR}/roms/${d}/*.hex"
                  "${CMAKE_SOURCE_DIR}/roms/${d}/*.BIN" "${CMAKE_SOURCE_DIR}/roms/${d}/*.bin"
                  "${CMAKE_SOURCE_DIR}/roms/${d}/*.S19" "${CMAKE_SOURCE_DIR}/roms/${d}/*.s19"
                  "${CMAKE_SOURCE_DIR}/roms/${d}/*.SREC" "${CMAKE_SOURCE_DIR}/roms/${d}/*.srec")
    list(APPEND ROM_IMAGE_FILES ${img})
    # The SHOW ROMS description is read from roms/<NAME>/DESC, so editing one must
    # re-run the embed -- same as changing the image itself.
    file(GLOB descsrc CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/roms/${d}/DESC")
    list(APPEND ROM_IMAGE_FILES ${descsrc})
  endif()
endforeach()

add_custom_command(
  OUTPUT "${ROMS_GENERATED}"
  COMMAND ${CMAKE_COMMAND}
          -DROMS_DIR=${CMAKE_SOURCE_DIR}/roms
          -DOUT=${ROMS_GENERATED}
          -P ${CMAKE_SOURCE_DIR}/cmake/embed_roms.cmake
  DEPENDS ${CMAKE_SOURCE_DIR}/cmake/embed_roms.cmake ${ROM_IMAGE_FILES}
  COMMENT "Embedding ROMs from roms/"
  VERBATIM)

# ---------------------------------------------------------------------------
# Built-in machines (DESIGN.md 10.0)
#
# machines/*.toml are BUILD-TIME INPUTS, exactly like a .cpp file. They are read
# here, at compile time, and emitted as byte arrays that link into .rodata.
#
# THE SHIPPED BINARY IS SELF-CONTAINED. It never looks for a machines/ directory,
# never consults an install prefix, and does not care where it was copied to --
# `pascal100 default` works from a USB stick. That is a hard requirement: this
# ships as ONE EXECUTABLE plus documentation, and a simulator that cannot find
# its own default machine is a simulator that does not start.
#
# The .toml files are still real TOML, in the real config format, because they
# are parsed at runtime by the same loadTomlText() that reads your config file.
# So they double as worked examples, and they cannot drift from the format.
# ---------------------------------------------------------------------------
set(MACHINES_GENERATED "${CMAKE_BINARY_DIR}/generated/machines_generated.cpp")
file(GLOB MACHINE_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/machines/*.toml")
# A REMOVED machine must rebuild the list too. DEPENDS on the files alone re-runs the embed
# when a file changes, not when one is deleted, and an old build directory kept the deleted
# machines (2026-11-14). file(CONFIGURE) rewrites the stamp only when the list changes.
set(MACHINES_LIST_STAMP "${CMAKE_BINARY_DIR}/generated/machines_list.txt")
list(JOIN MACHINE_FILES "\n" _machine_list)
file(CONFIGURE OUTPUT "${MACHINES_LIST_STAMP}" CONTENT "${_machine_list}\n")

add_custom_command(
  OUTPUT "${MACHINES_GENERATED}"
  COMMAND ${CMAKE_COMMAND}
          -DMACHINES_DIR=${CMAKE_SOURCE_DIR}/machines
          -DOUT=${MACHINES_GENERATED}
          -P ${CMAKE_SOURCE_DIR}/cmake/embed_machines.cmake
  DEPENDS ${CMAKE_SOURCE_DIR}/cmake/embed_machines.cmake ${MACHINE_FILES} "${MACHINES_LIST_STAMP}"
  COMMENT "Embedding machines from machines/"
  VERBATIM)

# ---------------------------------------------------------------------------
# Which commit this binary is.
#
# Between releases, MOST binaries are untagged: every CI artifact, every local
# build, anything handed to somebody to try. `pascal100 0.1.0` names all of them
# identically, so a bug report against one cannot be traced to the source that
# produced it. `git describe` can, and the build is the only place that knows.
#
# THE FALLBACK IS "unknown", AND THAT IS DELIBERATE. A release tarball has no .git
# in it, and neither does a source zip; there is nothing to describe and nothing
# to infer from. Writing PROJECT_VERSION there instead would produce a string that
# looks like provenance and is not -- and the way you find out it lied is by
# spending an afternoon on a bug in a revision the binary was never built from.
#
# CONFIGURE TIME, not build time: git runs once and the answer is baked into one
# generated header, so every object file in the binary agrees about it and no
# compile shells out. `git status` is consulted too, since
# "which commit" is a half-answer for a tree with uncommitted edits in it -- which
# is exactly the tree most hand-built binaries come from. Both are a snapshot; the
# CONFIGURE_DEPENDS below re-runs this when HEAD moves, and a build that only
# edited files still reports the dirty flag it was configured with. Re-run cmake if
# you need it exact.
# ---------------------------------------------------------------------------
find_package(Git QUIET)
set(PASCAL100_GIT_COMMIT "unknown")
set(PASCAL100_GIT_DIRTY 0)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
  execute_process(
    # --match: pascal100's own tags only (pascal100-v0.1.0, ...). The repo also holds
    # altairsim's v1.3.x tags from before the split; describing against those would name
    # an altairsim release (standalone step 4, 2026-11-09).
    COMMAND "${GIT_EXECUTABLE}" describe --tags --match "pascal100-v*" --always
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE git_describe
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE git_rc)
  # A non-zero status means git could not answer -- no commits yet, or this is not a
  # work tree after all. Leave it "unknown"; a half-captured sha is worse than none.
  if(git_rc EQUAL 0 AND git_describe)
    set(PASCAL100_GIT_COMMIT "${git_describe}")
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=no
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      OUTPUT_VARIABLE git_status
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
      RESULT_VARIABLE status_rc)
    # Untracked files are excluded on purpose: a build/ directory, an editor's swap
    # file and a scratch disk image are not modifications to the source this binary
    # was built from, and a dirty flag that is always set says nothing.
    if(status_rc EQUAL 0 AND git_status)
      set(PASCAL100_GIT_DIRTY 1)
    endif()
  endif()
  # So that a commit, a checkout or a rebase re-configures and the sha follows. HEAD
  # is the file that moves in all three; the ref it names moves when a commit lands on
  # the branch you are already on.
  #
  # ONLY FOR AN ORDINARY .git DIRECTORY. In a linked work tree `.git` is a FILE naming
  # the real gitdir elsewhere, so there is no .git/HEAD here to watch -- the version is
  # still captured correctly above (git itself follows the pointer), it just does not
  # re-configure by itself. Chasing that indirection would buy one convenience and a
  # second parser for git's on-disk layout.
  if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/.git" AND EXISTS "${CMAKE_SOURCE_DIR}/.git/HEAD")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                 "${CMAKE_SOURCE_DIR}/.git/HEAD")
    file(READ "${CMAKE_SOURCE_DIR}/.git/HEAD" head_contents)
    if(head_contents MATCHES "^ref: ([^\r\n]+)")
      set(head_ref "${CMAKE_SOURCE_DIR}/.git/${CMAKE_MATCH_1}")
      if(EXISTS "${head_ref}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${head_ref}")
      endif()
    endif()
  endif()
endif()

configure_file("${CMAKE_SOURCE_DIR}/cmake/version.h.in"
               "${CMAKE_BINARY_DIR}/generated/version_generated.h" @ONLY)

# ---------------------------------------------------------------------------
# The platform layer (DESIGN.md 2.1). NO #ifdefs -- ANYWHERE.
#
# OS differences are one interface header with zero conditionals in it, plus one
# implementation file per OS, and CMAKE picks the directory. This block is the
# entire mechanism, and it is the only place in the build that asks what OS it is
# on. Nothing in src/ ever does.
#
# The win32 half is FIELD-PROVEN, not merely written: serial against two real FTDI
# ports, socket against the real TCP stack, terminal against a real console, each a
# leg of `ctest -L hw`. docs/porting-notes.md records the traps that turned up.
#
# MSVC is the ONLY supported Windows toolchain -- it is what CI builds and what is shipped.
# Nothing here is knowingly MSVC-only (no _MSC_VER anywhere in src/, lowercase Windows headers,
# plain main()); MSVC is simply the only toolchain tested and shipped.
# ---------------------------------------------------------------------------
if(WIN32)
  set(PLATFORM_SOURCES
    src/platform/win32/home_win32.cpp
    src/platform/win32/localtime_win32.cpp
    src/platform/win32/pty_win32.cpp
    src/platform/win32/serial_win32.cpp
    src/platform/win32/socket_win32.cpp
    src/platform/win32/terminal_win32.cpp)
else()
  set(PLATFORM_SOURCES
    src/platform/posix/home_posix.cpp
    src/platform/posix/localtime_posix.cpp
    src/platform/posix/pty_posix.cpp
    src/platform/posix/serial_posix.cpp
    src/platform/posix/socket_posix.cpp
    src/platform/posix/terminal_posix.cpp)
  # A baud with no standard termios constant -- 76800, an S-100 staple -- is set through
  # an OS-specific ioctl: BOTHER/termios2 on Linux, IOSSIOSPEED on macOS. A genuine
  # per-OS split is a per-OS FILE, never an #ifdef in the shared serial_posix.cpp
  # (DESIGN.md 2.1). CMake picks the one that matches.
  if(APPLE)
    list(APPEND PLATFORM_SOURCES src/platform/posix/macos/serial_custombaud_macos.cpp)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND PLATFORM_SOURCES src/platform/posix/linux/serial_custombaud_linux.cpp)
  endif()
endif()

# ---------------------------------------------------------------------------
# ...AND THE LINT THAT KEEPS IT TRUE (DESIGN.md 2.1).
#
# The rule above is worth exactly as much as its enforcement. Without this, the first
# time somebody needs one small thing on Windows an #ifdef appears "just this once",
# and inside a month you are SIMH.
#
# It is a BUILD dependency of the library, not a test, because 2.1 says "fails the
# build" -- a rule you can merge and fix later is a rule you have already lost. It
# greps for OS macros AND for OS headers; cmake/lint_platform.cmake explains why the
# second half is not optional.
# ---------------------------------------------------------------------------
add_custom_target(platform_lint
  COMMAND ${CMAKE_COMMAND} -DROOT=${CMAKE_SOURCE_DIR}
                           -P ${CMAKE_SOURCE_DIR}/cmake/lint_platform.cmake
  COMMENT "DESIGN.md 2.1: checking the OS has not escaped src/platform/"
  VERBATIM)

# ---------------------------------------------------------------------------
# Core library -- everything except main(). Tests link this.
# ---------------------------------------------------------------------------
add_library(altair_core STATIC
  ${PLATFORM_SOURCES}
  src/core/version.cpp
  src/core/crc32.cpp
  src/core/statefile.cpp
  src/core/hex.cpp
  src/core/symbols.cpp
  src/core/paths.cpp
  src/core/bus.cpp
  src/core/busclock.cpp
  src/core/clock.cpp
  src/core/board.cpp
  src/core/machine.cpp
  src/core/roms.cpp
  src/core/machines.cpp
  src/core/value.cpp
  src/core/expr.cpp
  src/core/debug.cpp
  src/core/debuglog.cpp
  src/core/spindle.cpp
  src/isa/isa8080.cpp
  src/isa/isa8085.cpp
  src/isa/isaZ80.cpp
  src/isa/isaPcode.cpp
  src/isa/isa8088.cpp
  src/cpu/cpu8085.cpp
  src/cpu/cpu8088.cpp
  src/cpu/cpu80286.cpp
  src/cpu/cpuZ80.cpp
  src/cpu/pcodeOps.cpp
  src/cpu/cpuPmachine.cpp
  src/cpu/pmachineTiming.cpp
  src/host/stream.cpp
  src/host/filter.cpp
  src/host/console.cpp
  # The built-in terminal engine: an attributed grid (TerminalScreen), the painter that
  # renders it into a Display (TerminalRenderer), and the abstract dialect it drives
  # (TerminalEmulator, header-only). Lifted from the VDB-8024 board; the shared basis for
  # the generic terminal endpoint (issue #244). See src/host/terminal/.
  src/host/terminal/screen.cpp
  src/host/terminal/renderer.cpp
  # The generic terminal endpoint (issue #244): the VT100/ANSI dialect, the name->factory
  # table of emulations, and TerminalStream -- the engine wearing a ByteStream face, so
  # `connect = "terminal"` gives every serial board a windowed terminal for free.
  src/host/terminal/vt100.cpp
  src/host/terminal/adm3a.cpp
  src/host/terminal/vt52.cpp
  src/host/terminal/h19.cpp
  src/host/terminal/emulations.cpp
  src/host/terminal/stream.cpp
  src/host/endpoint.cpp
  src/host/hostserial.cpp
  src/host/tcp.cpp
  # `ModemLine` -- a software-controlled phone line: an inbound connection is a RING
  # held silent until the guest answers, an outbound dial happens on off-hook, and an
  # idle modem holds no sockets. The PMMI MM-103 (Phase 2) drives it; unlike tcp.cpp's
  # auto-answer server it never picks up on its own.
  src/host/modemline.cpp
  # `file:PATH` -- a host file as a write-only sink: a printout, or a capture of a
  # line. A SINK, so it is trivial next to hostserial/tcp (no pump, no read path).
  src/host/file.cpp
  # `printer:QUEUE` -- a host print queue as a write-only sink that buffers bytes into
  # JOBS (host/printer_stream.h, issue #70). Always compiled and pure: it knows how to
  # decide where one job ends but nothing about CUPS -- the platform submit backend is
  # linked separately and only when found (the printer feature block below).
  src/host/printer_stream.cpp
  # `ENDPOINT|FILE` -- a transparent hex TAP on any line (host/tee_stream.h). A
  # decorator ByteStream: it copies bytes past in both directions and forwards the rest,
  # observe-only and 8-bit clean, so it is exempt from the "one FilterStream" rule.
  # The Telnet NVT protocol (RFC 854) as a pure byte engine, no sockets -- shared by
  # the telnet: endpoint and the PMMI modem's answer/dial line (host/telnet_codec.h).
  src/host/telnet_codec.cpp
  # `telnet:PORT` / `telnet:HOST:PORT` -- socket: plus the Telnet protocol handshake
  # (host/telnet_stream.h). A decorator over TcpStream: on a new session it offers
  # WILL ECHO / SGA so a stock `telnet` client drops its local echo and goes
  # character-at-a-time, and it strips inbound IAC. socket: stays a raw pipe.
  src/host/telnet_stream.cpp
  src/host/tee_stream.cpp
  # `ENDPOINT|socket:PORT` -- a live BIDIRECTIONAL mirror of a line over TCP: a human
  # telnets in, watches the session, and can type back onto the wire (issue #381). The
  # tee's twin (same decorator shape) but the sink is a socket, not a file, and it is
  # two-way. The watcher never paces the guest (host/mirror_stream.h).
  src/host/mirror_stream.cpp
  # `pty` / `pty:LINK` -- a line on a pseudo-terminal, for a terminal program (issue #685).
  # The line itself, where `|pty` is a mirror of one: carrier follows the program.
  src/host/pty_stream.cpp
  src/host/media.cpp

  # A MediaFile that lives on a TNFS server (FujiNet's network filesystem), so a disk
  # or tape image mounts straight off the network: MOUNT ... tnfs://host/image.dsk. It
  # slots in behind openHostMedia like any other medium (src/host/tnfs.h).
  src/host/tnfs.cpp
  # A host DIRECTORY as one CF/microSD card: a `card.geometry` descriptor maps LBA runs
  # to per-volume backing files, streamed lazily (seek per block, never slurped like a
  # HostFile). openHostMedia() is the resolver both mains install (src/host/cardimg.h).
  src/host/cardimg.cpp
  src/host/disk.cpp
  # A video board's frame, OUT of the seam: resolved through its palette to a PPM image
  # and to a one-character-per-pixel text grid, so a test can diff a whole picture and a
  # person can open what the board drew (src/host/framedump.h). Pure functions over a
  # Surface; no board and no window involved.
  src/host/framedump.cpp
  # ImageDisk (.IMD) -> raw sibling .DSK, at the MOUNT layer. Pure (bytes in, bytes out),
  # so DiskImage stays raw-only and the container never reaches it (src/host/imd.h).
  src/host/imd.cpp
  # The sandboxed host directory the Host Bridge reaches through. It is here, and not
  # on the board, because the board must never see a path, a symlink or a file handle
  # -- and because the escape tests that matter are tests of THIS, with no card in
  # sight (DESIGN.md 7, src/host/hostdir.h).
  src/host/hostdir.cpp
  # The chips. A chip is not a card: it is modeled from its DATA SHEET, it knows
  # nothing about S-100, and it is shared by every card that has one soldered to it.
  src/chips/mc6850.cpp
  src/chips/sio2port.cpp
  src/chips/uart1602.cpp
  src/chips/wd17xx.cpp
  # The NEC uPD765A (Intel 8272) -- the CompuPro Disk 1A's FDC (src/chips/upd765.h).
  src/chips/upd765.cpp
  # A bit-banged RS-232 line with no UART on the board -- the CompuPro Disk 1's port 3 D7
  # (src/chips/bitserial.h).
  src/chips/bitserial.cpp
  src/chips/i8257.cpp
  # The OKI MSM5832 -- a BCD real-time clock/calendar reading the host's wall clock.
  # The CompuPro System Support 1 carries one (src/chips/msm5832.h).
  src/chips/msm5832.cpp
  src/chips/sig2651.cpp
  # The Signetics SCN2681 DUART -- two UART channels, a counter/timer, a 7-bit input and an
  # 8-bit output port. The CompuPro SPUZ carries one (src/chips/scn2681.h).
  src/chips/scn2681.cpp
  # The Intel 8253 -- a three-channel programmable interval timer, counts computed from
  # the clock rather than stepped. The CompuPro System Support 1 carries one.
  src/chips/intel8253.cpp
  # The Intel 8259A -- a programmable interrupt controller; INT is a pure function of the
  # live IR levels the card supplies. The CompuPro System Support 1 carries two (cascade).
  src/chips/intel8259a.cpp
  # The AMD Am9511A (Intel 8231A) and Am9512 (Intel 8232) math chips -- the bus side they
  # share (ports, byte stack, END/SVREQ/ERR), then each chip's command set: the 9511A's
  # fixed, float and derived functions, the 9512's single and double precision floats.
  # The CompuPro System Support 1's math socket takes either.
  src/chips/am95xx.cpp
  src/chips/am9511a.cpp
  src/chips/am9512.cpp
  src/boards/s100-memory.cpp
  # The generic Z80 CPU card -- the 88-CPU's twin (mits-z80cpu.h), carrying a
  # CpuZ80 instead of a Cpu8080. Listed after it so the reader sees the pair.
  src/boards/mits-z80cpu.cpp
  # The Digicomp Pascal-100: a Z80 and the WD MicroEngine on one card (pascal100 Stage 4).
  src/boards/digicomp-pascal100.cpp
  src/boards/compupro-cpu8588.cpp
  src/boards/compupro-cpu8687.cpp
  src/boards/compupro-cpu286.cpp
  src/boards/mits-8085cpu.cpp
  # The CompuPro System Support 1 -- a multifunction card. Being brought up in phases
  # (issue #392); today the OKI MSM5832 real-time clock. See src/boards/compupro-ss1.h.
  src/boards/compupro-ss1.cpp
  # The CompuPro Disk 1A -- a uPD765A floppy controller with its own DMA and a boot EPROM over
  # PHANTOM*, on the base it shares with the Disk 1. See src/boards/compupro-disk.h.
  src/boards/compupro-disk.cpp
  src/boards/compupro-disk1.cpp
  src/boards/compupro-disk1a.cpp
  src/boards/compupro-disk2.cpp
  src/boards/compupro-disk3.cpp
  src/boards/compupro-mdriveh.cpp
  src/boards/compupro-selch.cpp
  src/boards/compupro-interfacer1.cpp
  src/boards/compupro-interfacer3.cpp
  src/boards/compupro-interfacer4.cpp
  src/boards/mits-2sio.cpp
  # SerialBoardBase -- the reusable 88-SIO B serial engine (UART, inverted status word, VI
  # straps, interrupt enables, decode, connector, snapshot). Not a registered board; the
  # 88-SIO is it plus a name, the cassette engine extends it. See serial-board-base.h.
  # SSM PB1 -- a 2708/2716 EPROM programmer + on-board EPROM board. A hybrid decode: a 4K
  # programming-socket window + one arm/type control port, plus a read-only on-board PROM area.
  # Tarbell #1011 (SD) / #2022 (DD) -- one card carrying a WD FD177x floppy controller
  # AND a 32-byte boot PROM that shadows low RAM over PHANTOM*. See src/boards/tarbell.h.
  src/boards/tarbell.cpp
  src/boards/jade-dd.cpp
  src/boards/floppy-drive.cpp
  src/boards/io4.cpp           # SSM IO-4: real chip-backed dual-UART serial board (2P+2S; serial section)
  # OUR OWN CARD -- the only one in this list that is not a period board, and the
  # first genuinely new hardware built against the board API (DESIGN.md 12.1).
  src/boards/hostbridge.cpp
  src/boards/registry.cpp
  src/config/toml.cpp
  src/cli/monitor.cpp
  src/cli/commands.cpp
  src/cli/lineedit.cpp
  src/util/json.cpp
  src/mcp/server.cpp
  "${ROMS_GENERATED}"
  "${MACHINES_GENERATED}")

target_include_directories(altair_core PUBLIC src "${CMAKE_BINARY_DIR}/generated")

# The 8088's instruction set: the x86 core of Clock Signal (CLK), MIT, vendored in
# third_party/clk (README.pascal100.md there). Its own library, and a SYSTEM include
# directory, so CLK's warnings do not fail our warnings-as-errors build; only
# src/cpu/cpu8088.cpp and src/cpu/cpu80286.cpp include it (through src/cpu/x86_impl.h).
add_library(clk_x86 STATIC
  third_party/clk/InstructionSets/x86/Decoder.cpp
  third_party/clk/InstructionSets/x86/Instruction.cpp)
target_include_directories(clk_x86 SYSTEM PUBLIC third_party/clk)
if(MSVC)
  target_compile_options(clk_x86 PRIVATE /W0)
else()
  target_compile_options(clk_x86 PRIVATE -w)
endif()
target_link_libraries(altair_core PUBLIC clk_x86)
# MSVC reports two back-end warnings inside CLK's header templates, which a SYSTEM include
# does not hide because cpu8088.cpp and cpu80286.cpp instantiate them: C4715 ("not all
# control paths return a value": a switch that returns on every enum case) and C4702
# (unreachable code, from `if constexpr` branches). Neither is a defect. They are turned off
# for those two files only.
if(MSVC)
  set_source_files_properties(src/cpu/cpu8088.cpp src/cpu/cpu80286.cpp
                              PROPERTIES COMPILE_OPTIONS "/wd4715;/wd4702")
endif()

# ---------------------------------------------------------------------------
# The optional video backend: SDL3 (DESIGN.md 7.4). THE FIRST OPTIONAL LINK-TIME
# DEPENDENCY, and it is DETECTED, not required.
#
# pascal100's loudest claim is that it builds with a C++20 compiler and CMake and
# nothing else -- so a graphics library cannot be a hard dependency. This block
# auto-detects an installed SDL3: found, and the video boards get a real window;
# absent, and they build headless against a NullDisplay (src/host/display_null.h)
# and every test still passes. Either way the build configures and succeeds. See
# docs/devguide/ for how to install SDL3.
#
# PASCAL100_ENABLE_SDL is ON by default (= "use SDL3 if it is here"); set it OFF to
# force a headless build even where SDL3 is installed. That is what a macOS UNIVERSAL
# build needs *against a Homebrew SDL3*, which is single-arch (arm64) and will not link
# into a fat binary. It is NOT a limit of SDL3: upstream's SDL3.xcframework carries a
# macos-arm64_x86_64 slice that is genuinely universal, so a universal windowed build is
# possible -- link that rather than brew's dylib.
#
# EXACTLY ONE CI LEG HAS SDL3: macOS, which installs it from Homebrew and builds native
# arm64. It is therefore the only place display_sdl.cpp is compiled, and the workflow
# FAILS that leg if it comes up headless -- otherwise the coverage lapses in silence,
# which is what happened before. Linux and Windows still take the not-found path.
#
# (Through v0.2.0 no leg had SDL3 at all, so nothing compiled display_sdl.cpp and every
# shipped binary was headless. This comment also used to claim the macOS leg passed
# -DPASCAL100_ENABLE_SDL=OFF; it never did.)
#
# ONLY THE src/host/*_sdl.cpp FILES INCLUDE SDL: the display. The boards
# see host/display.h and never learn a window or a
# sound device exists, so they compile in every configuration; only those translation
# units and the composition root (src/main.cpp) are macro-gated.
# SDL is cross-platform, not an OS header, so it does not trip the 2.1 platform_lint.
# ---------------------------------------------------------------------------
option(PASCAL100_ENABLE_SDL "Build the SDL3-backed video boards if SDL3 is found" ON)
if(PASCAL100_ENABLE_SDL)
  find_package(SDL3 CONFIG QUIET)
  if(SDL3_FOUND)
    target_sources(altair_core PRIVATE src/host/display_sdl.cpp)
    target_link_libraries(altair_core PUBLIC SDL3::SDL3)
    target_compile_definitions(altair_core PUBLIC PASCAL100_ENABLE_SDL)

    # Whether the process may be brought to the foreground, and when (2.1: one file per
    # OS, CMake picks; src/platform/foreground.h says why it is needed at all). It is
    # selected HERE, inside the SDL block, rather than alongside the other platform
    # sources, because only a video board can raise the question -- so a headless build
    # never compiles it and macOS never links AppKit for a simulator with no window.
    if(APPLE)
      enable_language(OBJCXX)
      target_sources(altair_core PRIVATE src/platform/posix/macos/foreground_macos.mm)
      target_link_libraries(altair_core PUBLIC "-framework AppKit")
    elseif(WIN32)
      target_sources(altair_core PRIVATE src/platform/win32/foreground_win32.cpp)
    else()
      target_sources(altair_core PRIVATE src/platform/posix/foreground_posix.cpp)
      # dlopen, for libX11 at run time: X11 hands the keyboard back to the terminal at a
      # stop, and loading Xlib late keeps it out of the build and the package.
      target_link_libraries(altair_core PRIVATE ${CMAKE_DL_LIBS})
    endif()

    message(STATUS "SDL3 found -- video boards enabled (windowed)")
  else()
    message(STATUS "SDL3 not found -- video boards build headless (null display). "
                   "Install SDL3 (see docs/devguide) for a window.")
  endif()
endif()

# ---------------------------------------------------------------------------
# The optional host-printer backend (docs/printing.md; issue #70). THE SECOND
# optional link-time dependency, and it follows the SDL3 rule above exactly: ON by
# default meaning "use it if it is here", a quiet detect, a message either way, and a
# build without it where `printer:` is simply not in the endpoint grammar and the
# resolver says so by name -- the one endpoint that cannot work on a given host fails
# at CONNECT with a sentence, not at compile.
#
# host/printer_stream.cpp is NOT here: it is pure (it decides job boundaries, it does
# not talk to a spooler) and is always compiled, so its boundary policy is tested with
# a fake sink on every platform, CUPS or no CUPS. Only the platform submit TU and the
# PASCAL100_ENABLE_PRINTER define that lights up the resolver arm are gated.
#
# macOS AND Linux are the SAME leg: both sit on CUPS, so one file (printer_cups.cpp)
# serves both and is selected with find_package(Cups) -- CMake's own module, which
# gives the imported Cups::Cups target. NOT pkg_check_modules: pkg-config is not
# guaranteed on a macOS dev box (this one has cups-config, not pkg-config), and a
# REQUIRED pkg check there would fail to configure a build that should just quietly
# skip the feature. Windows is WinSpool and is a later branch (issue #70 Phase 3).
option(PASCAL100_ENABLE_PRINTER "Print to a host queue if CUPS/WinSpool is found" ON)
if(PASCAL100_ENABLE_PRINTER)
  if(WIN32)
    # Phase 3 -- the file does not exist yet, so the feature is off on Windows until
    # it lands. Left here as the shape it will take (WinSpool, StartDocPrinter RAW).
    message(STATUS "printer: endpoint not yet built on Windows (issue #70 Phase 3)")
  else()
    find_package(Cups QUIET)
    if(Cups_FOUND)
      target_sources(altair_core PRIVATE src/platform/posix/printer_cups.cpp)
      target_link_libraries(altair_core PUBLIC Cups::Cups)
      target_compile_definitions(altair_core PUBLIC PASCAL100_ENABLE_PRINTER)
      message(STATUS "CUPS found -- printer: endpoint enabled")
    else()
      message(STATUS "CUPS not found -- printer: endpoint disabled. "
                     "Install libcups2-dev (Linux) for host printing.")
    endif()
  endif()
endif()

# Nothing links until the OS is where it belongs.
add_dependencies(altair_core platform_lint)

# The win32 socket layer (src/platform/win32/socket_win32.cpp) pulls in Winsock;
# link it. POSIX needs no equivalent -- sockets live in libc. This if(WIN32) is a
# build-script OS conditional, which is fine: the 2.1 lint governs C++ source, not
# CMake. PUBLIC so the test/tool executables that link altair_core inherit it.
if(WIN32)
  target_link_libraries(altair_core PUBLIC ws2_32)
endif()

add_executable(pascal100 src/main.cpp)
target_link_libraries(pascal100 PRIVATE altair_core)

# ---------------------------------------------------------------------------
# The manual's reference chapters, printed from the binary's own reflection layer.
#
# It links altair_core rather than shelling out to `pascal100` because no monitor command
# emits what a parameter table needs -- BOARDS TYPES has the help and not the defaults,
# SHOW has the defaults and not the help. See tools/gen-reference.cpp for the whole
# argument. It is a TOOL, not a feature: the shipping command surface is a contract, and
# a doc generator has no business in it.
#
# `docs-reference` rewrites docs/manual/ref/*.md and docs/monitor/ref/commands.md in the
# source tree, on purpose -- those files are committed, so that a reader of the repo (and the
# PDF build) needs no toolchain to see them. `docs-reference-check` below is what stops them
# going stale.
# ---------------------------------------------------------------------------
add_executable(altair_genref tools/gen-reference.cpp)
target_link_libraries(altair_genref PRIVATE altair_core)


add_custom_target(docs-reference
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_SOURCE_DIR}/docs/manual/ref"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_SOURCE_DIR}/docs/monitor/ref"
  COMMAND altair_genref "${CMAKE_SOURCE_DIR}/docs/manual/ref" "${CMAKE_SOURCE_DIR}/docs/monitor/ref"
  COMMENT "Generating docs/manual/ref/ and docs/monitor/ref/ from the binary")

# ---------------------------------------------------------------------------
# Tests -- no framework; a test is an executable that returns non-zero.
# ---------------------------------------------------------------------------
enable_testing()
add_executable(altair_tests
  tests/main.cpp
  tests/test_bus.cpp
  tests/test_ieee696.cpp
  tests/test_clock.cpp
  tests/test_busclock.cpp
  tests/test_statefile.cpp
  tests/test_snapshot.cpp
  tests/test_memory.cpp
  tests/test_paths.cpp
  tests/test_hex.cpp
  tests/test_srec.cpp
  tests/test_symbols.cpp
  tests/test_media.cpp
  tests/test_tnfs.cpp
  tests/test_cardimg.cpp
  tests/test_imd.cpp
  tests/test_roms.cpp
  tests/test_phantom.cpp
  tests/test_cli.cpp
  tests/test_console.cpp
  tests/test_debuglog.cpp
  tests/test_lineedit.cpp
  tests/test_boundary.cpp
  tests/test_numbers.cpp
  tests/test_units.cpp
  tests/test_machines.cpp
  tests/test_isa.cpp
  tests/test_z80_isa.cpp
  tests/test_pmachine.cpp
  tests/test_8085_isa.cpp
  tests/test_8088_isa.cpp
  tests/test_cpu.cpp
  tests/test_status.cpp
  tests/test_z80_cpu.cpp
  tests/test_8085_cpu.cpp
  tests/test_expr.cpp
  tests/test_debug.cpp
  tests/test_coreswitch.cpp
  tests/test_cpu8588.cpp
  tests/test_cpu8687.cpp
  tests/test_cpu286.cpp
  tests/test_pascal100.cpp
  tests/test_ddt.cpp
  tests/test_dma.cpp
  tests/test_sio2.cpp
  tests/test_io4.cpp
  tests/test_interfacer1.cpp
  tests/test_interfacer3.cpp
  tests/test_interfacer4.cpp
  tests/test_boardlog.cpp
  tests/test_printer.cpp
  tests/test_tee.cpp
  tests/test_mirror.cpp
  tests/test_ptystream.cpp
  tests/test_terminal.cpp
  tests/test_multiwindow.cpp
  tests/framecheck.cpp
  tests/test_framedump.cpp
  tests/test_lines.cpp
  tests/test_modemline.cpp
  tests/test_telnet.cpp
  tests/test_wd17xx.cpp
  tests/test_upd765.cpp
  tests/test_bitserial.cpp
  tests/test_i8257.cpp
  tests/test_scn2681.cpp
  tests/test_am9511a.cpp
  tests/test_am9512.cpp
  tests/test_tarbell.cpp
  tests/test_jadedd.cpp
  tests/test_cpu8088.cpp
  tests/test_cpu80286.cpp
  tests/test_disk1.cpp
  tests/test_disk1a.cpp
  tests/test_disk2.cpp
  tests/test_disk3.cpp
  tests/test_mdriveh.cpp
  tests/test_spindle.cpp
  tests/test_ss1.cpp
  tests/test_hostdir.cpp
  tests/test_hostbridge.cpp
  tests/test_mcp.cpp)
target_link_libraries(altair_tests PRIVATE altair_core)
# A couple of sections mount a real committed disk image (the SDOS video boot in
# test_vdb8024.cpp); `ctest` runs `unit` from the build tree, so give it the source root
# to resolve those paths -- the same trick as altair_cputest's ALTAIR_CPU_TEST_DIR.
target_compile_definitions(altair_tests PRIVATE ALTAIR_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME unit COMMAND altair_tests)

# ---------------------------------------------------------------------------
# The serial-line hardware gate. TWO REAL PORTS, A NULL MODEM BETWEEN THEM.
#
# Everything else in this project can be proved with a ScriptedStream and a
# MemoryMedia, and should be. Modem control cannot: whether RTS on one card really
# raises CTS on another is a question about a CABLE, and the only honest way to
# answer it is to send a volt down one.
#
# So this test is REAL HARDWARE, and it is opt-in -- CI has no serial ports and
# neither does a fresh checkout. Point it at the two ends of a null modem:
#
#   ALTAIR_SERIAL_A=/dev/tty.usbserial-AL009KFH \
#   ALTAIR_SERIAL_B=/dev/tty.usbserial-AB0NW409 ctest -L hw
#
# Unset, it SKIPS and says so. A hardware test that silently passes when the
# hardware is absent is worse than no hardware test at all -- so it exits 77 and
# SKIP_RETURN_CODE turns that into a `Skipped` in ctest's summary. Without this the
# run reads `serial-hw ... Passed` on a machine with no serial ports at all, which
# is a green tick for a cable nobody plugged in.
# ---------------------------------------------------------------------------
add_executable(altair_serialtest tests/serialtest.cpp)
target_link_libraries(altair_serialtest PRIVATE altair_core)
add_test(NAME serial-hw COMMAND altair_serialtest)
set_tests_properties(serial-hw PROPERTIES LABELS hw SKIP_RETURN_CODE 77)

# ---------------------------------------------------------------------------
# The SOCKET half of the same idea, against the real kernel TCP stack. The `unit`
# aggregate's test_lines.cpp already proves the loopback happy path; this proves the
# NON-BLOCKING CONNECT, and above all the REFUSED connect that surfaces in select()'s
# except set -- the one Winsock behaviour docs/porting-notes.md flagged as most likely
# wrong. It uses only platform/socket.h, so it needs no hardware and no opt-in: a free
# loopback port is always there. Labelled `hw` because, like serialtest, it touches the
# real world (the kernel) rather than a ScriptedStream -- so `ctest -L hw` runs the
# whole real-world leg at once.
# ---------------------------------------------------------------------------
add_executable(altair_sockettest tests/sockettest.cpp)
target_link_libraries(altair_sockettest PRIVATE altair_core)
add_test(NAME socket-hw COMMAND altair_sockettest)
set_tests_properties(socket-hw PROPERTIES LABELS hw)

# ---------------------------------------------------------------------------
# The TERMINAL half, on a real console. The pipe path is proven in CI already
# (acceptance-basic4k/8k drive readInput's pipe branch and its broken-pipe EOF with a
# live guest); this proves the CONSOLE path a piped test can never reach -- SetConsoleMode
# raw mode, ENABLE_VIRTUAL_TERMINAL_INPUT, and the PeekConsoleInput peek-and-discard loop
# that keeps readInput non-blocking (porting-notes' #1 most-likely-wrong item).
#
# It lives in src/platform/win32/ and NOT in tests/ on purpose: proving that loop with no
# human means MANUFACTURING console input (WriteConsoleInput), which is a Win32 call, and
# DESIGN.md 2.1 keeps OS calls in the platform layer. A test of OS-specific code is
# OS-specific code -- so it sits beside the file it tests, is WIN32-only, and SKIPs (77)
# where no console can be taken. The POSIX terminal has its own mirror of it, below.
# ---------------------------------------------------------------------------
if(WIN32)
  add_executable(altair_terminaltest src/platform/win32/terminaltest_win32.cpp)
  target_link_libraries(altair_terminaltest PRIVATE altair_core)
  add_test(NAME terminal-hw COMMAND altair_terminaltest)
  set_tests_properties(terminal-hw PROPERTIES LABELS hw SKIP_RETURN_CODE 77)
else()
  # The POSIX mirror, and it is not optional any more. "Proven by hand against a pty"
  # is what let issue #25 through: an empty read on a VMIN=0 tty returns 0, the same
  # as a hung-up pipe, and reporting both as the end of input latched Console::eof_ on
  # every interactive run. It failed for a user in a terminal and passed here and in
  # CI, every time, because a piped stdin never takes that branch. This one opens a
  # pty and takes it. It SKIPs (77) where no pty can be had.
  add_executable(altair_terminaltest src/platform/posix/terminaltest_posix.cpp)
  target_link_libraries(altair_terminaltest PRIVATE altair_core)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(altair_terminaltest PRIVATE util)  # openpty
  endif()
  add_test(NAME terminal-hw COMMAND altair_terminaltest)
  set_tests_properties(terminal-hw PROPERTIES LABELS hw SKIP_RETURN_CODE 77)
endif()

# ---------------------------------------------------------------------------
# CP/M off a REAL TNFS server. test_tnfs.cpp (in `unit`) proves the client against a fake
# server in RAM; this boots the CP/M example with its floppy mounted over UDP from de-tnfsd,
# SAVEs a file, and reads it back through a fresh mount. de-tnfsd is POSIX-only and not ours,
# so the script SKIPs (77) unless it is on PATH -- installing it turns the test on. `hw` for
# the same reason as serial-hw: it needs something outside the tree.
# ---------------------------------------------------------------------------
if(NOT WIN32)
  add_test(NAME tnfs-hw
           COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/tnfs-hw.sh
                   $<TARGET_FILE:pascal100> ${CMAKE_CURRENT_SOURCE_DIR}
                   ${CMAKE_CURRENT_BINARY_DIR}/tnfs-hw-work)
  set_tests_properties(tnfs-hw PROPERTIES LABELS hw SKIP_RETURN_CODE 77 TIMEOUT 120)
endif()

# ---------------------------------------------------------------------------
# The 8080 validation gate (DESIGN.md 3.2). Period CP/M test suites, run against
# our core. Separate from the unit tests because these are the tests we did NOT
# write, which is the entire reason they are worth anything.
#
# They read their .COM files off disk at runtime rather than being embedded --
# the ONE EXECUTABLE rule is about `pascal100`, and this is not it.
# ---------------------------------------------------------------------------
add_executable(altair_cputest tests/cputest.cpp)
target_link_libraries(altair_cputest PRIVATE altair_core)
target_compile_definitions(altair_cputest
  PRIVATE ALTAIR_CPU_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/cpu")

# The 8085 gate (tests/cputest.cpp). The 8085 is a documented-8085 superset of the
# 8080, flag-identical to it EXCEPT for ANA/ANI's auxiliary carry -- so its deep
# gate is 8085EXM, Ian Bartholomew's 8085 port of the exerciser with CRCs read off
# real 8085 silicon (tests/cpu/PROVENANCE.md), not 8080EXM. TST8080 and 8080PRE are
# 8080/8085 diagnostics and gate the 8085 unchanged; CPUTEST is not in the 8085 gate
# (it checks logical-AND's AC against the 8080 value, which a faithful 8085 fails --
# see cputest.cpp). cpu-8085exm is another ~10-minute, 2.9-billion-instruction run,
# so it is `slow` like cpu-8080exm.
add_test(NAME cpu-8085-tst8080 COMMAND altair_cputest 8085:TST8080)
add_test(NAME cpu-8085-8080pre COMMAND altair_cputest 8085:8080PRE)
add_test(NAME cpu-8085exm      COMMAND altair_cputest 8085:8085EXM)
set_tests_properties(cpu-8085exm PROPERTIES LABELS slow TIMEOUT 1800)

# The Z80 exercisers -- the same gate, one core down. ZEXDOC validates documented
# behavior; ZEXALL additionally pins the undocumented F5/F3 bits. Each is ~46
# billion instructions, so both are `slow` and skipped by `ctest -LE slow`.
add_test(NAME cpu-zexdoc COMMAND altair_cputest ZEXDOC)
add_test(NAME cpu-zexall COMMAND altair_cputest ZEXALL)
set_tests_properties(cpu-zexdoc cpu-zexall PROPERTIES LABELS slow TIMEOUT 3600)

# The CompuPro Disk 1A: boot CompuPro CP/M 2.2LD off a real 8" disk through the board's own
# boot EPROM, then read and write files on it. If it fails, the board is wrong, not the disk.
add_test(NAME acceptance-disk1a
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/disk1a.cmake)
set_tests_properties(acceptance-disk1a PROPERTIES LABELS acceptance TIMEOUT 200)

# The Jade Double D: Jade's own FORMAT.COM makes double-density, single-density and 3740 disks,
# reformats a disk to a smaller layout and two tracks of another, and the new disk boots.
add_test(NAME acceptance-jadedd-format
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/jadedd-format.cmake)
set_tests_properties(acceptance-jadedd-format PROPERTIES LABELS acceptance TIMEOUT 150)

# The CompuPro Disk 1: boot CompuPro CP/M 2.2N off a real 8" disk through builtin:DISK1
# (the factory EPROM, routine 4), then read and write files on it.
add_test(NAME acceptance-disk1
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/disk1.cmake)
set_tests_properties(acceptance-disk1 PROPERTIES LABELS acceptance TIMEOUT 140)

# The CompuPro M-Drive/H: CP/M 2.2N counts one board, formats drive M:, and PIP [V] writes a
# file there that runs (tests/acceptance/mdriveh-cpm22n.cmake).
add_test(NAME acceptance-mdriveh
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/mdriveh-cpm22n.cmake)
set_tests_properties(acceptance-mdriveh PROPERTIES LABELS acceptance TIMEOUT 140)

# The IEEE 696 bus (Stage 2): CP/M 2.2N on the Disk 1 machine,
# once with 64K of aliasing memory and once with 128K of 16-bit memory in two segments.
add_test(NAME acceptance-ieee696
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/ieee696.cmake)
set_tests_properties(acceptance-ieee696 PROPERTIES LABELS acceptance TIMEOUT 140)

# The Pascal-100 (Stage 5): UCSD Pascal III.0 boots on the Pascal-100 machine (tests/media/pascal100) to the command
# line, and the Filer lists the volumes and the boot volume. It boots a copy of
# tests/media/pascal100/pascal-dd.dsk, or of the disk that PASCAL100_BOOT_DISK names.
add_test(NAME acceptance-pascal100-boot
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/pascal100-boot.cmake)
set_tests_properties(acceptance-pascal100-boot PROPERTIES LABELS acceptance TIMEOUT 240)

# The System Support 1 math socket: the SS-1 manual's 9511A test program, assembled and run
# under CP/M 2.2N, prints OK with a 9511A fitted and ERROR with the socket empty.
add_test(NAME acceptance-ss1-math
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/ss1-math.cmake)
set_tests_properties(acceptance-ss1-math PROPERTIES LABELS acceptance TIMEOUT 180)

# ---------------------------------------------------------------------------
# THE SSM IO-4: the shipped example machine boots the SSM 8080 monitor on a real IO-4
# serial console, in the layout a user gets (no repository beside it).
add_test(NAME acceptance-io4
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/io4.cmake)
set_tests_properties(acceptance-io4 PROPERTIES LABELS acceptance TIMEOUT 120)

# ---------------------------------------------------------------------------
# THE HOST BRIDGE: a file crosses into CP/M and comes back byte for byte.
#
# TWO TESTS, ONE SCRIPT (-DMODE picks which). Both boot the built-in `default` (Disk 1,
# CompuPro CP/M 2.2N) off a scratch copy of the TRACKED tests/media/compupro/CPM22N.DSK, so
# both run on a fresh clone and in CI with nothing to download.
#
#   acceptance-hostbridge        THE CARD. LOADs the utilities from the .HEX that is checked
#                                in, and moves files -- including 256 bytes holding every
#                                value 00..FF, compared on the HOST's disk. About 1 second.
#
#   acceptance-hostbridge-build  THE SOURCES. Assembles R/W/HDIR.ASM from scratch inside the
#                                machine (PIP through the console, ASM, LOAD) and proves the
#                                result is bit-for-bit the .COM/.HEX/.PRN committed beside
#                                them. About 4 seconds.
#
# pascal100 2026-11-09: upstream ran build mode by hand only, because it needed an 8.6 MB
# image that is not in git. On CPM22N.DSK it needs none (the script erases what it does not
# need from its scratch copy), so it is registered, and a stale .ASM is caught on every run.
# ---------------------------------------------------------------------------
add_test(NAME acceptance-hostbridge
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -DMODE=fast
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/hostbridge.cmake)
# ~2s. The 60 is a BACKSTOP, not a budget -- and the script has its own 30s leash
# inside it, which fails with "THE MACHINE HUNG" instead of a confusing missing-string
# error. A test with a generous timeout is a test that can waste an hour of your evening.
set_tests_properties(acceptance-hostbridge PROPERTIES LABELS acceptance TIMEOUT 60)

# pascal100 2026-11-09: acceptance-hostbridge-attrs (W on CP/M 3, where OPEN hands a file's
# R/O/SYS/ARCHIVE bits back in the FCB) is removed with the SD Systems boards it booted on.
# Restore it when a CompuPro CP/M 3 is available.

add_test(NAME acceptance-hostbridge-build
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -DMODE=build
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/hostbridge.cmake)
# ~4s. The script's own leash for this mode is 180s; the 240 is the backstop above it.
# (By hand, the paths must be ABSOLUTE: a relative BIN is read against the -s script's
# folder and the MOUNT misses.)
set_tests_properties(acceptance-hostbridge-build PROPERTIES LABELS acceptance TIMEOUT 240)

# ---------------------------------------------------------------------------
# THE 8086 HOST BRIDGE UTILITIES (R.CMD, W.CMD, HDIR.CMD) on CompuPro CP/M 8-16, off a scratch
# copy of the tracked tests/media/compupro/CPM816.DSK. The same two modes as above, in their own
# script (tests/acceptance/hostbridge86.cmake). Each command is typed only after the A> prompt
# is back, over MCP: ASM86 takes any key typed ahead as a USER BREAK.
#
#   acceptance-hostbridge86        GENCMD each committed .H86 into its .CMD, prove the .CMD is
#                                  the committed one, and move files (the 256-byte round trip,
#                                  T and B, a wildcard into a folder, the sandbox). ~10 s.
#   acceptance-hostbridge86-build  ASM86 and GENCMD each .A86, and prove the .CMD, .H86 and .LST
#                                  are the committed ones. ~65 s: PIP echoes 55 KB of source
#                                  through the CP/M 8-16 console, one character at a time. `slow`.
# ---------------------------------------------------------------------------
add_test(NAME acceptance-hostbridge86
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -DMODE=fast
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/hostbridge86.cmake)
set_tests_properties(acceptance-hostbridge86 PROPERTIES LABELS acceptance TIMEOUT 300)
add_test(NAME acceptance-hostbridge86-build
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -DMODE=build
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/hostbridge86.cmake)
set_tests_properties(acceptance-hostbridge86-build PROPERTIES LABELS "acceptance;slow" TIMEOUT 400)
# acceptance-hostbridge86-mpm: the MP/M lock. Two users start a long R at once on the mpm8-16
# example (scratch copy); one waits for the other, and both files come back byte for byte.
# `slow`: ~11 s on an M-series Mac, but 126 s on the Windows CI runner, where it ran beside
# acceptance-hostbridge86 and pushed that one past its 120 s leash (2026-11-15).
add_test(NAME acceptance-hostbridge86-mpm
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/hostbridge86-mpm.cmake)
set_tests_properties(acceptance-hostbridge86-mpm PROPERTIES LABELS "acceptance;slow" TIMEOUT 600)

# ---------------------------------------------------------------------------
# WHICH MACHINE DID I GET? ./pascal100.toml, found in the working directory.
#
# The one file the simulator FINDS rather than is GIVEN -- and the only thing keeping
# that from being the trap looksLikeFile() refuses to walk into is that it happens ONLY
# on a command line that names nothing. So most of this test is the negative controls:
# `pascal100 basic4k`, `-m default` and `-n` must all be stone deaf to the directory
# they run in. If they ever are not, `pascal100 basic4k` has started meaning different
# things in different places, which is precisely the 2am bug the CLI is arranged to
# prevent -- reintroduced by the back door, by a convenience.
# ---------------------------------------------------------------------------
add_test(NAME acceptance-cwdconfig
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/cwdconfig.cmake)
set_tests_properties(acceptance-cwdconfig PROPERTIES LABELS acceptance TIMEOUT 60)

# ~/pascal100.ini runs on a plain interactive launch and on nothing else: not --mcp, not -x,
# not -s, whose stdout is a transport or a CI transcript.
add_test(NAME acceptance-userinit
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/userinit.cmake)
set_tests_properties(acceptance-userinit PROPERTIES LABELS acceptance TIMEOUT 60)

# A file name with a space, run unquoted, is two arguments: the error says names, not
# machines, states the rule and says how to start from a built-in (issue #712).
add_test(NAME acceptance-cli-args
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/cli-args.cmake)
set_tests_properties(acceptance-cli-args PROPERTIES LABELS acceptance TIMEOUT 60)

# The per-directory command-history file is written only for a human at a terminal; a
# script (-x/-s), a pipe, or --mcp must leave the directory untouched. This proves that
# negative -- the whole of the guarantee (see the file's own header for the pty/pipe trap).
add_test(NAME acceptance-history
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/history.cmake)
set_tests_properties(acceptance-history PROPERTIES LABELS acceptance TIMEOUT 60)

# Under --mcp, stdout is the JSON-RPC transport: a machine file's `#>` notes must go to
# stderr, not ahead of the `initialize` reply where a strict client chokes on them (#459).
add_test(NAME acceptance-mcp-stdout
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/mcp-stdout.cmake)
set_tests_properties(acceptance-mcp-stdout PROPERTIES LABELS acceptance TIMEOUT 60)

# `--log FILE` under --mcp writes the console to a text file a person can follow (#666):
# refused without --mcp, refused at startup when the file cannot be opened, and started
# empty each session.
add_test(NAME acceptance-mcp-log
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/mcp-log.cmake)
set_tests_properties(acceptance-mcp-log PROPERTIES LABELS acceptance TIMEOUT 120)

# `--mcp` refuses -x, -s and -i (#737): it handed the machine to the server before the code
# that uses them, so they were accepted and dropped with exit status 0.
add_test(NAME acceptance-mcp-refuses-exec
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/mcp-refuses-exec.cmake)
set_tests_properties(acceptance-mcp-refuses-exec PROPERTIES LABELS acceptance TIMEOUT 60)

# `--set "cpu0 clock_hz=2000000"` is a SET run before the machine file's `startup` (#736):
# a -x "SET ..." runs after it, when a machine that starts itself has already booted.
add_test(NAME acceptance-set-option
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/set-option.cmake)
set_tests_properties(acceptance-set-option PROPERTIES LABELS acceptance TIMEOUT 60)

# `--mirror pty` under --mcp mirrors the console to a pseudo-terminal (#683): the session
# says where the link is and removes it at exit; on Windows it is refused at startup.
add_test(NAME acceptance-mcp-mirror-pty
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/mcp-mirror-pty.cmake)
set_tests_properties(acceptance-mcp-mirror-pty PROPERTIES LABELS acceptance TIMEOUT 120)

# The `pty` endpoint -- a serial line on a pseudo-terminal (#685): CONNECT says where the
# link is, a machine file puts it in its own folder; on Windows CONNECT refuses it by name.
add_test(NAME acceptance-pty-endpoint
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/pty-endpoint.cmake)
set_tests_properties(acceptance-pty-endpoint PROPERTIES LABELS acceptance TIMEOUT 120)

# ---------------------------------------------------------------------------
# SNAPSHOT / RESTORE through the real monitor -- the command wiring (DESIGN.md 13).
# The state round-trip is proved at the unit level (tests/test_snapshot.cpp); this
# proves the two commands write and read a file the operator named, and refuse one
# that is not a snapshot.
# ---------------------------------------------------------------------------
add_test(NAME acceptance-snapshot
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/snapshot.cmake)
set_tests_properties(acceptance-snapshot PROPERTIES LABELS acceptance TIMEOUT 60)

# ---------------------------------------------------------------------------
# THE EXAMPLES, IN THE LAYOUT WE ACTUALLY SHIP THEM IN.
#
# examples/ is the product, not a fixture: a user gets the binary and that tree, and NOT this
# repository. This test proves the EXAMPLES work -- every machine file in each one, run from
# a copy outside the tree -- and nothing else. No test of the simulator may read the examples
# (examples-isolation, below, enforces it); a test that needs media takes it from tests/media.
# HAVE_SDL tells it which answer cpm22.toml's windowed console must give on this build.
# ---------------------------------------------------------------------------
if(SDL3_FOUND)
  set(examples_have_sdl 1)
else()
  set(examples_have_sdl 0)
endif()
add_test(NAME acceptance-examples
         COMMAND ${CMAKE_COMMAND}
                 -DSIM=$<TARGET_FILE:pascal100>
                 -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -DHAVE_SDL=${examples_have_sdl}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/examples.cmake)
set_tests_properties(acceptance-examples PROPERTIES LABELS acceptance TIMEOUT 120)

# ...and the rule that makes that true: no test script, fixture or test source may name
# examples/ in a line of code (comments may). Text only, so it runs on every leg.
add_test(NAME examples-isolation
         COMMAND ${CMAKE_COMMAND}
                 -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/no-example-deps.cmake)
set_tests_properties(examples-isolation PROPERTIES LABELS acceptance TIMEOUT 60)

# ...and the half that TYPES is a pty test, registered further down with the other
# expect ones. That split is not organisational: a pipe hands the guest keystrokes it
# has not asked for, which mangles a command line whenever the host wins the race.

# ---------------------------------------------------------------------------
# THE MANUAL'S REFERENCE MAY NOT DRIFT FROM THE BINARY.
#
# docs/manual/ref/*.md is committed (a reader of the repo, and the PDF build, should need
# no toolchain to see it) and generated (a hand-typed parameter table is a second schema,
# and the first draft of one in this very project got three of the memory card's eight
# defaults wrong). Committed + generated = it can rot, so it is tested.
# ---------------------------------------------------------------------------
add_test(NAME docs-reference
         COMMAND ${CMAKE_COMMAND}
                 -DGEN=$<TARGET_FILE:altair_genref>
                 -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/docs-reference.cmake)
set_tests_properties(docs-reference PROPERTIES LABELS acceptance TIMEOUT 60)

# ---------------------------------------------------------------------------
# THE USER MANUAL MAY NOT NAME ANYTHING THE READER DOES NOT HAVE.
#
# It ships in the zip, next to the binary and the disks. There is no src/, no CMakeLists and
# no DESIGN.md in there. "Self-contained" erodes one helpful cross-reference at a time, in
# good faith, and nobody notices until a user follows one -- so it is a test, not a habit.
# The Developer Guide is deliberately exempt: it is FOR the source.
# ---------------------------------------------------------------------------
add_test(NAME docs-manual
         COMMAND ${CMAKE_COMMAND}
                 -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/docs-manual.cmake)
set_tests_properties(docs-manual PROPERTIES LABELS acceptance TIMEOUT 60)

# ---------------------------------------------------------------------------
# THE PACKAGE MAP AND THE MANUAL MUST NAME THE SAME PACKAGE.
#
# docs-manual (above) is a blacklist: it proves the manual names nothing OUTSIDE the package,
# but never reads docs/package.map. This is the other half -- the cheap one: a text comparison
# proving docs/package.map (the producer's manifest) and docs/manual/package.md (the reader's
# manifest) ship the same set. It caught the manual promising examples/debugger while the map
# did not. The EXPENSIVE half -- assemble the zip and run the manual's commands against it --
# still needs build-package.sh under a workflow and is deliberately not here.
# ---------------------------------------------------------------------------
add_test(NAME docs-package
         COMMAND ${CMAKE_COMMAND}
                 -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/docs-package.cmake)
set_tests_properties(docs-package PROPERTIES LABELS acceptance TIMEOUT 60)

# The skill's long reference file must open with its Contents (Anthropic's skill guidance:
# over 100 lines needs a table of contents at the top). The list is hand-typed, so this proves
# it still names every `##` section. The cheatsheet's list is generated and needs no check.
add_test(NAME docs-skill-contents
         COMMAND ${CMAKE_COMMAND}
                 -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/docs-skill-contents.cmake)
set_tests_properties(docs-skill-contents PROPERTIES LABELS acceptance TIMEOUT 60)

# ---------------------------------------------------------------------------
# THE RELEASE WORKER'S CHECKS MUST READ WHAT THE TOOLS REALLY PRINT.
#
# tools/release-worker.sh decides STOP or go on a release from lines of text: ctest's pass
# line, the SDL3 line of the configure, `--version`, the verify-package PASS line. In 1.3.0 a
# hand-written check read only the last 6 lines of the ctest output and reported a STOP on a
# pass (issue #643). --self-test runs the checks on sample output, including that case. It
# builds nothing. No `sh`, no test -- and we say so, as for `expect` below.
# ---------------------------------------------------------------------------
find_program(SH_EXECUTABLE sh)
if(SH_EXECUTABLE)
  add_test(NAME release-worker-selftest
           COMMAND ${SH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/release-worker.sh --self-test)
  set_tests_properties(release-worker-selftest PROPERTIES
                       LABELS acceptance TIMEOUT 60
                       PASS_REGULAR_EXPRESSION "release-worker self-test: PASS")
else()
  message(STATUS "sh not found -- the release-worker-selftest test is not registered")
endif()

# ---------------------------------------------------------------------------
# THE WATCH SCRIPT WE SHIP SEES A LIVE MIRROR.
#
# tools/user/mirror-watch.sh goes into the package as tools/mirror-watch.sh (issue #664): the
# viewer a person runs to watch an assistant drive a machine. This starts it BEFORE the
# simulator, as its README allows, runs a ROM monitor over --mcp --mirror, and reads what the
# watcher showed -- that it waited, connected, showed the guest, and waited again when the
# simulator stopped. `amon` because it prints a banner with no media and no key.
#
# bash, not sh: the script connects through bash's /dev/tcp. There is no bash test on Windows;
# mirror-watch.ps1 is that platform's script, and it is checked by hand before a release.
# ---------------------------------------------------------------------------
find_program(BASH_EXECUTABLE bash)
if(BASH_EXECUTABLE AND NOT WIN32)
  add_test(NAME mirror-watch
           COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/mirror-watch.sh
                   $<TARGET_FILE:pascal100>
                   ${CMAKE_CURRENT_SOURCE_DIR}/tools/user/mirror-watch.sh
                   # pascal100 2026-11-09: upstream used the built-in `amon`; ALTMON is a test
                   # fixture here, and its ROM is at F800 (63488) too.
                   ${CMAKE_CURRENT_SOURCE_DIR}/tests/media/altmon/altmon.toml 63488 "ALTMON")
  set_tests_properties(mirror-watch PROPERTIES
                       LABELS acceptance TIMEOUT 120
                       PASS_REGULAR_EXPRESSION "mirror-watch: PASS")
else()
  message(STATUS "bash not found (or Windows) -- the mirror-watch test is not registered")
endif()

# ---------------------------------------------------------------------------
# THE LBR TOOLS WE SHIP OPEN A REAL LIBRARY.
#
# tools/user/lbr-extract.py, uncrunch.py and unsqueeze.py go into the package as tools/. This
# extracts and decodes two period CP/M libraries, one crunched and one squeezed, and compares
# the files with lbrate's output, then checks that a bad CRC and a bad checksum are reported. Python 3 runs them on every platform; without it the
# test is not registered.
# ---------------------------------------------------------------------------
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME lbr-tools
           COMMAND ${CMAKE_COMMAND}
                   -DPYTHON=${Python3_EXECUTABLE}
                   -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                   -DBIN=${CMAKE_CURRENT_BINARY_DIR}
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/lbr-tools.cmake)
  set_tests_properties(lbr-tools PROPERTIES LABELS acceptance TIMEOUT 120)
else()
  message(STATUS "Python 3 not found -- the lbr-tools test is not registered")
endif()

# ---------------------------------------------------------------------------
# EVERY reference/ DISTILLATION MUST BE CATALOGUED.
#
# reference/*.md are the text-only hardware references. reference/README.md indexes them and
# docs/sources.md records the period scan each was distilled from -- both hand-maintained, and
# both went stale silently (four un-indexed files at once, caught only by a hand-run diff).
# This globs the distillations and fails if any is missing from either catalogue. Text only.
# ---------------------------------------------------------------------------
add_test(NAME reference-index
         COMMAND ${CMAKE_COMMAND}
                 -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/reference-index.cmake)
set_tests_properties(reference-index PROPERTIES LABELS acceptance TIMEOUT 60)

# ---------------------------------------------------------------------------
# The PDFs. NOT part of the build, and it must never become part of it.
#
# pascal100's loudest claim is that it builds with a C++20 compiler and CMake and nothing
# else. A PDF wants pandoc and a browser, and neither is allowed anywhere near the thing that
# produces the simulator. So this is a target you ASK for, and its absence is not an error --
# `cmake -S . -B build` on a machine with no pandoc must still configure and still build.
#
# YOU DO NOT HAVE TO RUN THIS ANY MORE, and you do not have to remember to commit what it
# makes: .github/workflows/docs.yml rebuilds both PDFs on every push to master that touches
# docs/, and commits them if their text actually moved. Nobody remembering was exactly the
# problem -- the shipped developer guide spent two days insisting the project had no CI.
#
# This target is still worth having: it is how you SEE a change before you push it, and the
# output is the same as CI's, because the fonts travel inside the document (docs/fonts/) and
# CI pins the same pandoc. Committing its output is fine and so is not committing it -- if
# your PDF matches, CI keeps it; if it does not, CI replaces it. What you must not do is
# hand-edit a PDF, or expect the committed one to stay as you built it.
# ---------------------------------------------------------------------------
add_custom_target(docs
  COMMAND "${CMAKE_SOURCE_DIR}/tools/build-docs.sh"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Building pascal100-manual.pdf and pascal100-devguide.pdf"
  VERBATIM)
add_dependencies(docs docs-reference)

# ---------------------------------------------------------------------------
# THE INTERACTIVE ONE. Everything above drives the CLI through a PIPE, and a pipe is not
# a terminal -- so three things that only exist on a tty go untested by all of them: the
# echoed prompt and line editing, ATTN (^E) taking the console back from a running guest,
# and the run loop's decision to pace against a wall clock. This test types.
#
# It needs `expect`, which ships with macOS and is a package everywhere else. If it is not
# there we do not register the test rather than fail the suite over a missing tool -- and
# we SAY SO at configure time, because a test that silently is not there is worse than one
# that fails.
# ---------------------------------------------------------------------------
find_program(EXPECT_EXECUTABLE expect)
if(EXPECT_EXECUTABLE)
  add_test(NAME acceptance-cli
           COMMAND ${EXPECT_EXECUTABLE}
                   -f ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/cli.exp
                   $<TARGET_FILE:pascal100>)
  set_tests_properties(acceptance-cli PROPERTIES LABELS acceptance TIMEOUT 60)

  # A guest at a prompt keeps its crystal's time (#606). It is an EXPECT test because the idle
  # nap and the throttle are both gated on a real terminal; a piped run does neither. The
  # bound has one side -- the guest gains no more than 1.1 times the wall time -- so a loaded
  # runner, which only makes a paced guest slower, cannot fail it.
  add_test(NAME acceptance-clock-idle-pace
           COMMAND ${EXPECT_EXECUTABLE}
                   -f ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/clock-idle-pace.exp
                   $<TARGET_FILE:pascal100>)
  set_tests_properties(acceptance-clock-idle-pace PROPERTIES LABELS acceptance TIMEOUT 60)

  # ---------------------------------------------------------------------------
  # THE 88-2SIO, DRIVEN BY REAL PERIOD SOFTWARE -- polled AND interrupt-driven.
  #
  # Two of Mike Douglas's 2SIO test programs (tests/programs/2sio, see PROVENANCE.md), a
  # dozen bytes each. ECHO polls; ECHOINT enables receive interrupts and then does nothing
  # but NOP -- so every character it echoes has to travel the whole chain, with the guest
  # never touching a register. If the receiver only advanced when a register was read,
  # ECHOINT would echo nothing for ever.
  #
  # The script carries its OWN negative control (ECHOINT with the jumper out must go deaf),
  # so this one test cannot pass for the wrong reason and needs no WILL_FAIL twin.
  #
  # It is FAST -- no tape, no disk, no monitor -- which is the point: `ps2int` covers the
  # same wire, but it takes a cassette boot and 2.5K of MITS monitor to say so.
  add_test(NAME acceptance-2sio-echo
           COMMAND ${EXPECT_EXECUTABLE}
                   -f ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/2sio-echo.exp
                   $<TARGET_FILE:pascal100>)
  set_tests_properties(acceptance-2sio-echo PROPERTIES
                       LABELS acceptance TIMEOUT 120
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  # ---------------------------------------------------------------------------
  # THE TARBELL FLOPPY CONTROLLERS -- single density (#1011) and double density (#2022).
  #
  # Each carries its own 32-byte boot PROM, so the boot is AUTOMATIC: RESET arms the PROM at
  # 0000, it reads track 0 through the FD177x and jumps into the cold loader (releasing
  # PHANTOM* on the A5-high fetch), and CP/M 2.2 comes up with no monitor and nothing typed.
  # An expect test because DIR must be typed at the A> prompt (a pipe cannot). The DD case is
  # the mixed-density proof: an SD track 0 then DD tracks 1-76, which hangs mid-load if the
  # card ever builds the wrong FD177x part. Both mount the tracked master WRITE-PROTECTED and
  # shasum it -- the boot only reads.
  # ---------------------------------------------------------------------------
  add_test(NAME acceptance-tarbell
           COMMAND ${EXPECT_EXECUTABLE}
                   -f ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/tarbell.exp
                   $<TARGET_FILE:pascal100>)
  set_tests_properties(acceptance-tarbell PROPERTIES
                       LABELS acceptance TIMEOUT 90
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  add_test(NAME acceptance-tarbelldd
           COMMAND ${EXPECT_EXECUTABLE}
                   -f ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/tarbelldd.exp
                   $<TARGET_FILE:pascal100>)
  set_tests_properties(acceptance-tarbelldd PROPERTIES
                       LABELS acceptance TIMEOUT 90
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  # The same #2022, booting a disk whose CBIOS is assembled DMACNTL=TRUE: post-boot sector
  # reads move through the on-card Intel 8257, so the card masters the S-100 bus (DESIGN.md
  # 4.5) instead of an IN/OUT byte loop. Reaching A> and reading a DIR is the whole-machine
  # proof the DMA path works; the byte-level cycle-steal proof is in tests/test_tarbell.cpp.
  add_test(NAME acceptance-tarbelldd-dma
           COMMAND ${EXPECT_EXECUTABLE}
                   -f ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/tarbelldd-dma.exp
                   $<TARGET_FILE:pascal100>)
  set_tests_properties(acceptance-tarbelldd-dma PROPERTIES
                       LABELS acceptance TIMEOUT 90
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  # ---------------------------------------------------------------------------
  # THE JADE DOUBLE D boots Jade CP/M 2.2 Release 2 through the DDBOOT host PROM at F000,
  # which starts the board's own Z80 to read the system. Once with Jade's 20K master (SD
  # data tracks), once with a 56K double-density disk. Both are mounted WRITE-PROTECTED and
  # shasummed.
  # ---------------------------------------------------------------------------
  add_test(NAME acceptance-jadedd
           COMMAND ${EXPECT_EXECUTABLE}
                   -f ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/jadedd.exp
                   $<TARGET_FILE:pascal100>
                   ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/jadedd.toml
                   ${CMAKE_CURRENT_SOURCE_DIR}/tests/media/jade/JADEDD-CPM22-MASTER-SSSD.DSK
                   "20K CP/M" STAT)
  set_tests_properties(acceptance-jadedd PROPERTIES
                       LABELS acceptance TIMEOUT 90
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  add_test(NAME acceptance-jadedd-dd
           COMMAND ${EXPECT_EXECUTABLE}
                   -f ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/jadedd.exp
                   $<TARGET_FILE:pascal100>
                   ${CMAKE_CURRENT_SOURCE_DIR}/tests/acceptance/jadedd-dd.toml
                   ${CMAKE_CURRENT_SOURCE_DIR}/tests/media/jade/jadedd-dd-sysgen-56k.dsk
                   "56K CP/M" DDBIOS56)
  set_tests_properties(acceptance-jadedd-dd PROPERTIES
                       LABELS acceptance TIMEOUT 90
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  # TWO GEOMETRIES, ONE CONTROLLER -- acceptance-dcdd-mixed -- WAS HERE, and was removed
  # on 2026-07-19 with the 8 MB image it needed. An 8 MB disk in drive 0 and a 77-track
  # floppy in drive 2 at once (FDC+ manual 3.7.4) was the only proof that the format and the
  # Spindle are PER DRIVE rather than per card, and it broke as CORRUPTION rather than as an
  # error, which is why it weighed the bytes. It required an 8.6 MB download, so it skipped
  # on every fresh clone and in all of CI. Rebuilding that coverage without the download is an
  # open task, not a settled one.

else()
  message(STATUS "expect not found -- SKIPPING every interactive (pty-driven) acceptance "
                 "test, not just acceptance-cli: none of the tests that type at a guest are "
                 "registered without expect (as on Windows). The tty and mid-run paths "
                 "(prompt echo, ATTN, clock pacing, typed guest commands, interrupt-driven "
                 "console input) go uncovered here.")
endif()

# An operator's own ~/pascal100.ini would run on every interactive launch an acceptance
# test makes and print `ini>` lines into what the test reads. Point HOME at a folder with
# no such file, for every acceptance test, so a developer's machine and CI agree. (Set last,
# so it covers every test added above.)
get_property(_all_tests DIRECTORY PROPERTY TESTS)
foreach(_t IN LISTS _all_tests)
  # tnfs-hw launches the binary interactively too, but is not named acceptance-*.
  if(_t MATCHES "^acceptance-" OR _t STREQUAL "tnfs-hw")
    set_property(TEST ${_t} APPEND PROPERTY ENVIRONMENT
                 "HOME=${CMAKE_CURRENT_BINARY_DIR}/no-home"
                 "USERPROFILE=${CMAKE_CURRENT_BINARY_DIR}/no-home")
  endif()
endforeach()
