# THE EXAMPLES, IN THE LAYOUT A USER ACTUALLY GETS.
#
# examples/ is not a test fixture. It is what we SHIP: a user gets the `pascal100` binary and
# that tree, and nothing else -- no repository, no build directory, no tests. So this file
# does not ask "does the simulator work" -- the simulator's own tests answer that from their
# own fixtures (tests/media, media.cmake), and none of them may read examples/. It asks:
#
#     DOES THE THING WE HAND PEOPLE WORK WHERE WE HAND IT TO THEM?
#
# So it COPIES each example out of the tree, leaving the repository behind, and runs EVERY
# machine file in it from there -- the only arrangement in which a repo-root-relative path in
# an example is visible as the bug it is.
#
# Expects: -DSIM=<pascal100> -DSRC=<source dir> -DBIN=<binary dir> -DHAVE_SDL=<0|1>

set(dist "${BIN}/examples-work")
file(REMOVE_RECURSE "${dist}")

# The whole of an MCP session, sent up front: every `run` waits for its `until` before the
# next request is read, so the file is a script, not a race.
function(mcp_session file)
  set(lines [=[{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"examples","version":"1"}}}
{"jsonrpc":"2.0","method":"notifications/initialized"}]=])
  set(id 2)
  foreach(req ${ARGN})
    string(APPEND lines "\n{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tools/call\",\"params\":${req}}")
    math(EXPR id "${id} + 1")
  endforeach()
  file(WRITE "${file}" "${lines}\n")
endfunction()

function(expect_contains out why)
  math(EXPR last "${ARGC} - 1")
  foreach(i RANGE 2 ${last})
    string(FIND "${out}" "${ARGV${i}}" hit)
    if(hit LESS 0)
      message(FATAL_ERROR "examples: ${why}\n"
                          "  '${ARGV${i}}' never reached the terminal.\n--- output ---\n${out}")
    endif()
  endforeach()
endfunction()

# pascal100 2026-11-09: sections 1 and 3 (examples/cpm22, examples/ai-mcp) boot CompuPro CP/M 2.2K
# (cpm22k-64k.dsk) on the default machine; altairsim's MITS disk and 88-DCDD are not in this
# build. Sections 4 and 5 (BASIC off cassette) went with tape support.
# ---- 1. examples/cpm22 -- THE QUICK START'S OWN MACHINE, and every other file beside it. ------
#
# `examples/cpm22` is the flagship: it is what docs/manual/quick-start.md promises, and until
# 2026-07-19 NO test booted it. `acceptance-dcdd-readonly` boots a test-owned machine file
# that happens to mount the same image, which proves the CARD and proves nothing at all
# about the example -- and that is much of how the manual drifted as far as it did.
#
# The keys are cpm-dir.keys, and its first byte is a NUL for the same reason basic4k.keys's
# is: every keystroke is in the buffer before the machine is switched on, and CP/M's cold
# start clears the SIO's receive register and eats exactly one byte.
file(COPY "${SRC}/examples/cpm22" DESTINATION "${dist}/examples")
set(cpm "${dist}/examples/cpm22")

# WHY ONLY THE FIRST DIRECTORY LINE IS CLAIMED, and it is CP/M's behaviour, not a hedge:
# DIR polls the console between lines so the operator can stop a long listing, and ANY key
# already waiting aborts it. Our keys are all in the pipe before the machine starts, so the
# CR behind `DIR` stops the listing after one line -- every time, and on purpose, since the
# alternative is a test whose output depends on how fast the host is.
#
# One line is enough to be a real claim. `A: L80      COM` is a directory entry read off
# the image through the 88-DCDD: a machine that merely started prints the banner and stops.
function(expect_cpm out why)
  foreach(want "CompuPro Systems" "64K CP/M 2.2K" "A>" "A: MAC      COM")
    string(FIND "${out}" "${want}" hit)
    if(hit LESS 0)
      message(FATAL_ERROR "examples: ${why}\n"
                          "  '${want}' never reached the terminal.\n--- output ---\n${out}")
    endif()
  endforeach()
endfunction()

execute_process(
  COMMAND           "${SIM}" cpm22-console.toml
  WORKING_DIRECTORY "${cpm}"
  INPUT_FILE        "${SRC}/tests/acceptance/cpm-dir.keys"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
expect_cpm("${out}" "`cd examples/cpm22 && pascal100 cpm22-console.toml` did not boot CP/M")

# ...and by path from the distribution root, where the disk is NOT. Same machine, and the
# floppy has to be found beside the file that names it rather than beside the operator.
execute_process(
  COMMAND           "${SIM}" examples/cpm22/cpm22-console.toml
  WORKING_DIRECTORY "${dist}"
  INPUT_FILE        "${SRC}/tests/acceptance/cpm-dir.keys"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
expect_cpm("${out}" "`pascal100 examples/cpm22/cpm22-console.toml` from the dist root did not boot CP/M")

# A script run by -s from the dist root finds the floppy beside the SCRIPT, not the shell's folder
# (#575). The script is the three lines the manual's machines chapter shows, written into the copy.
file(WRITE "${cpm}/boot.cmd"
     "MACHINE default\nMOUNT fd0:drive0 \"cpm22k-64k.dsk\"\nRUN 0\n")
execute_process(
  COMMAND           "${SIM}" -s examples/cpm22/boot.cmd
  WORKING_DIRECTORY "${dist}"
  INPUT_FILE        "${SRC}/tests/acceptance/cpm-dir.keys"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
file(REMOVE "${cpm}/boot.cmd")
expect_cpm("${out}" "`pascal100 -s examples/cpm22/boot.cmd` from the dist root did not boot CP/M")

# The README's `--set` command (#736): the same machine at 2 MHz, with the file as it is. The
# setting is applied before `startup> RUN 0`, CP/M boots and lists the disk, and SHOW CLOCK
# afterwards reports the pacing that the boot had. Without --set the file gives full speed --
# that is the control, and it is what makes `paced` a claim about the option.
execute_process(
  COMMAND           "${SIM}" cpm22-console.toml --set "cpu0 clock_hz=2000000" -x "SHOW CLOCK"
  WORKING_DIRECTORY "${cpm}"
  INPUT_FILE        "${SRC}/tests/acceptance/cpm-dir.keys"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
expect_cpm("${out}" "`pascal100 cpm22-console.toml --set \"cpu0 clock_hz=2000000\"` did not boot CP/M")
string(FIND "${out}" "set> SET cpu0 clock_hz=2000000" at_set)
string(FIND "${out}" "startup> RUN 0" at_startup)
if(at_set LESS 0 OR at_startup LESS 0 OR NOT at_set LESS at_startup)
  message(FATAL_ERROR "examples: --set was not applied before `startup> RUN 0`\n--- output ---\n${out}")
endif()
if(NOT out MATCHES "pacing +paced")
  message(FATAL_ERROR "examples: with --set \"cpu0 clock_hz=2000000\", SHOW CLOCK does not say `paced`\n--- output ---\n${out}")
endif()
execute_process(
  COMMAND           "${SIM}" cpm22-console.toml -x "SHOW CLOCK"
  WORKING_DIRECTORY "${cpm}"
  INPUT_FILE        "${SRC}/tests/acceptance/cpm-dir.keys"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
if(NOT out MATCHES "pacing +free")
  message(FATAL_ERROR "examples: without --set, SHOW CLOCK does not say `free` (the --set check proves nothing)\n--- output ---\n${out}")
endif()

# cpm22.toml puts the console in the built-in windowed VT100. A build with SDL loads it
# -- under SDL's dummy video driver, so no window opens on a test machine -- and the console
# line must hold the terminal. A headless build (no SDL) must REFUSE it, with the one message
# that says why: the spec parsed, and only the missing window stopped it. Each leg asserts the
# answer its own build must give; neither passes on the other's.
mcp_session("${dist}/terminal.jsonl"
  [=[{"name":"monitor","arguments":{"command":"SHOW if1"}}]=])
execute_process(
  COMMAND           "${CMAKE_COMMAND}" -E env SDL_VIDEODRIVER=dummy
                    "${SIM}" cpm22.toml --mcp
  WORKING_DIRECTORY "${cpm}"
  INPUT_FILE        "${dist}/terminal.jsonl"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           30
)
if(HAVE_SDL)
  expect_contains("${out}" "`pascal100 cpm22.toml` did not load on a build with SDL"
                  "if1  (interfacer1)" "serial  terminal?emulation=vt100&size=80x24")
else()
  expect_contains("${out}" "a headless build did not refuse cpm22.toml for its missing window"
                  "terminal: this build has no window")
endif()

# ---- 1b. examples/pascal -- UCSD Pascal III.0 on the Pascal-100. --------------------------------
#
# The README's own steps on the console twin, from a COPY (the guest can write): boot to the
# Command: line, F for the Filer, L and * to list the boot volume, a space past the listing's
# pause, Q back to the Command: line. MCP does not run the startup, so `run from` 0. A prompt
# line ends in the version tag F1-2.0: waiting for it reads the whole line. (Not `]`: a square
# bracket in a CMake list argument stops the list from splitting.)
file(COPY "${SRC}/examples/pascal" DESTINATION "${dist}/examples")
set(pas "${dist}/examples/pascal")
mcp_session("${dist}/pascal.jsonl"
  [=[{"name":"run","arguments":{"from":0,"until":"Command:","timeout_ms":60000}}]=]
  [=[{"name":"run","arguments":{"until":"F1-2.0","timeout_ms":10000}}]=]
  [=[{"name":"run","arguments":{"input":"F","until":"Filer:","timeout_ms":10000}}]=]
  [=[{"name":"run","arguments":{"until":"F1-2.0","timeout_ms":10000}}]=]
  [=[{"name":"run","arguments":{"input":"L","until":"vol ?","timeout_ms":10000}}]=]
  [=[{"name":"run","arguments":{"input":"*\r","until":"continue","timeout_ms":20000}}]=]
  [=[{"name":"run","arguments":{"input":" ","until":"Filer:","timeout_ms":20000}}]=]
  [=[{"name":"run","arguments":{"until":"F1-2.0","timeout_ms":10000}}]=]
  [=[{"name":"run","arguments":{"input":"Q","until":"Command:","timeout_ms":10000}}]=])
execute_process(
  COMMAND           "${SIM}" pascal-console.toml --mcp
  WORKING_DIRECTORY "${pas}"
  INPUT_FILE        "${dist}/pascal.jsonl"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           120
)
# The Command: line arrives in two run results, so its pieces are checked one by one.
expect_contains("${out}" "the pascal README's steps did not go as it says"
                "U.C.S.D.  Pascal  System  III.0" "Command: E(d" "F(ile" "Filer:"
                "WORK:" "SYSTEM.FILER" "SYSTEM.COMPILER")

# pascal.toml puts the console in the built-in VT52 window. As for cpm22.toml: a build
# with SDL loads it under the dummy video driver, a headless build refuses it for the window.
mcp_session("${dist}/pascal-vt52.jsonl"
  [=[{"name":"monitor","arguments":{"command":"SHOW if1"}}]=])
execute_process(
  COMMAND           "${CMAKE_COMMAND}" -E env SDL_VIDEODRIVER=dummy
                    "${SIM}" pascal.toml --mcp
  WORKING_DIRECTORY "${pas}"
  INPUT_FILE        "${dist}/pascal-vt52.jsonl"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           30
)
if(HAVE_SDL)
  expect_contains("${out}" "`pascal100 pascal.toml` did not load on a build with SDL"
                  "if1  (interfacer1)" "serial  terminal?emulation=vt52&size=80x24")
else()
  expect_contains("${out}" "a headless build did not refuse pascal.toml for its missing window"
                  "terminal: this build has no window")
endif()

# ---- 1c. examples/cpm86, cpm8-16, mpm8-16, cdos8-16 -- CompuPro CP/M-86 (CPU 86/87), 8-16 (CPU 8085/88)
# and Concurrent DOS 8-16 (CPU 286). --
#
# Each console twin boots from a COPY over MCP (MCP does not run the startup: `run from` 0, or
# from FFFF0 = 1048560, the 8086's reset address, for cpm86) and
# runs DIR. The strings are the ones each system printed when the example was made: a banner from
# CPM.SYS or MPM.SYS proves the 8088 runs, and a directory entry proves the floppy (or the M20
# through the Disk 2 and Selector Channel) reads. Each window file must load under SDL's dummy
# driver on a build with SDL, and be refused for its window on a headless build.
function(example_816 dir console window want_win)
  file(COPY "${SRC}/examples/${dir}" DESTINATION "${dist}/examples")
  set(here "${dist}/examples/${dir}")
  mcp_session("${dist}/${dir}-win.jsonl"
    [=[{"name":"monitor","arguments":{"command":"BOARDS"}}]=])
  execute_process(
    COMMAND           "${CMAKE_COMMAND}" -E env SDL_VIDEODRIVER=dummy
                      "${SIM}" ${window} --mcp
    WORKING_DIRECTORY "${here}"
    INPUT_FILE        "${dist}/${dir}-win.jsonl"
    OUTPUT_VARIABLE   out
    ERROR_VARIABLE    out
    TIMEOUT           30
  )
  if(HAVE_SDL)
    expect_contains("${out}" "`pascal100 ${window}` did not load on a build with SDL" "${want_win}")
  else()
    expect_contains("${out}" "a headless build did not refuse ${window} for its missing window"
                    "terminal: this build has no window")
  endif()
endfunction()

example_816(cpm86 cpm86-console.toml cpm86.toml "cpu8687")
mcp_session("${dist}/cpm86.jsonl"
  [=[{"name":"run","arguments":{"from":1048560,"until":"A>","timeout_ms":60000}}]=]
  [=[{"name":"run","arguments":{"input":"DIR\r","until":"A>","timeout_ms":20000}}]=])
execute_process(
  COMMAND           "${SIM}" cpm86-console.toml --mcp
  WORKING_DIRECTORY "${dist}/examples/cpm86"
  INPUT_FILE        "${dist}/cpm86.jsonl"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           120
)
expect_contains("${out}" "the cpm86 example did not boot CP/M-86 as its README says"
                "CP/M 86 1.1PAA" "Total Memory: 1 MEGABYTE" "DDT86    CMD")

# CP/M 8-16 1.1PD from the Q540: the Disk 3 BIOS finds the board, and A: is the hard disk.
example_816(cpm8-16 cpm816-console.toml cpm816.toml "disk3")
mcp_session("${dist}/cpm816d3.jsonl"
  [=[{"name":"run","arguments":{"from":0,"until":"A>","timeout_ms":60000}}]=]
  [=[{"name":"run","arguments":{"input":"DIR\r","until":"A>","timeout_ms":20000}}]=]
  [=[{"name":"run","arguments":{"input":"STAT A:\r","until":"A>","timeout_ms":20000}}]=])
execute_process(
  COMMAND           "${SIM}" cpm816-console.toml --mcp
  WORKING_DIRECTORY "${dist}/examples/cpm8-16"
  INPUT_FILE        "${dist}/cpm816d3.jsonl"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           120
)
expect_contains("${out}" "the cpm8-16 example did not boot CP/M 8-16 from the Disk 3 as its README says"
                "CompuPro CP/M 8-16 1.1PD" "A: DISK2    CMD : DISK240  CMD : DISK3    CMD" "A: RW, Free Space:     4,816k")

# MP/M from the M20: the boot floppy's hard-disk loader, MPM.SYS, then DIR of A: on the hard disk.
example_816(mpm8-16 mpm816-console.toml mpm816.toml "disk2")
mcp_session("${dist}/mpm816.jsonl"
  [=[{"name":"run","arguments":{"from":0,"until":"0A>","timeout_ms":120000}}]=]
  [=[{"name":"run","arguments":{"input":"DIR\r","until":"0A>","timeout_ms":60000}}]=]
  [=[{"name":"run","arguments":{"input":"SYNC\r","until":"0A>","timeout_ms":60000}}]=])
execute_process(
  COMMAND           "${SIM}" mpm816-console.toml --mcp
  WORKING_DIRECTORY "${dist}/examples/mpm8-16"
  INPUT_FILE        "${dist}/mpm816.jsonl"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           180
)
expect_contains("${out}" "the mpm8-16 example did not boot MP/M from the hard disk as its README says"
                "Hard Disk MP/M 8-16 Loader Rev. 1.0D." "CompuPro MP/M 8-16 V2.1F"
                "A: MPM      SYS" "A: TTYS" "System Files Exist")

# Concurrent DOS 8-16 4.1 on the CPU 286 from the Q540. The console twin puts tty1 (console 1,
# an Interfacer 4 port) on the console line: the shell's sign-on and 0A> come out there by
# themselves. DIR proves A: on the Disk 3, and HELLO80 on B: proves the SPUZ runs an 8080
# program. Both window files (color and mono PC Video) must load.
example_816(cdos8-16 cdos816-console.toml cdos816.toml "pcvideo")
example_816(cdos8-16 cdos816-console.toml cdos816-mono.toml "pcvideo")
mcp_session("${dist}/cdos816.jsonl"
  [=[{"name":"run","arguments":{"from":0,"until":"0A>","timeout_ms":120000}}]=]
  [=[{"name":"run","arguments":{"input":"DIR\r","until":"0A>","timeout_ms":60000}}]=]
  [=[{"name":"run","arguments":{"input":"B:HELLO80\r","until":"LOAD on the SPUZ","timeout_ms":60000}}]=])
execute_process(
  COMMAND           "${SIM}" cdos816-console.toml --mcp
  WORKING_DIRECTORY "${dist}/examples/cdos8-16"
  INPUT_FILE        "${dist}/cdos816.jsonl"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           180
)
expect_contains("${out}" "the cdos8-16 example did not boot Concurrent DOS from the Disk 3 as its README says"
                "CompuPro Concurrent DOS 8-16" "CCPM     SYS" "System files exist"
                "Hello from ASM and LOAD on the SPUZ")

# ---- 2. examples/debugger -- the walkthrough, symbols and hex loaded from beside the file. ----
#
# examples/debugger is a taught exercise: a 46-byte program, its .PRN listing and its .HEX,
# and a README that walks the monitor's debugger. This runs that walkthrough non-
# interactively, from the example's own directory, so it proves the README is true: the
# disassembly names labels and operands the way it says, and the .prn/.hex resolve beside
# the machine file rather than beside the repo.
file(COPY "${SRC}/examples/debugger" DESTINATION "${dist}/examples")
set(dbg "${dist}/examples/debugger")

execute_process(
  COMMAND           "${SIM}" debugger.toml
                    -x "SYMBOLS LOAD HELLO.PRN"
                    -x "LOAD HELLO.HEX"
                    -x "DISASM START-DONE"
                    -x "DISASM PUTC 7"
                    -x "EXAMINE START"
                    -x "BREAK DONE"
                    -x "RUN"
  WORKING_DIRECTORY "${dbg}"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           30
)

# What has to be true: the files loaded from beside the machine file; the disassembly is
# symbolic (a leading label, a label operand, and the EQU-address operand that is the whole
# point of the feature); and the program actually ran and printed through the 2SIO.
foreach(want
        "12 symbol(s) from HELLO.PRN"   # the .PRN resolved beside debugger.toml
        "loaded 46 bytes"               # so did the .HEX
        "START:"                        # a program label heads its own line
        "CALL PUTC"                     # a 16-bit operand reads as a label
        "LXI SP,STACK"                  # ...and as an EQU-address -- the CALL BDOS case
        "IN 10"                         # a BYTE operand stays a number (a port is not an address)
        "HELLO, WORLD"                  # it ran, on the console the file wired up
        "stopped at 0112")              # and stopped at the DONE breakpoint
  string(FIND "${out}" "${want}" hit)
  if(hit LESS 0)
    message(FATAL_ERROR "examples: the debugger walkthrough did not behave as the README says.\n"
                        "  '${want}' never reached the terminal.\n--- output ---\n${out}")
  endif()
endforeach()

# And the byte operand that must NOT be named: IN 10 stays IN 10, never IN TTYS, because a
# port is a byte and only a 16-bit operand is an address (README section 2).
string(FIND "${out}" "IN TTYS" hit)
if(hit GREATER_EQUAL 0)
  message(FATAL_ERROR "examples: a BYTE operand was annotated as a symbol.\n"
                      "  'IN 10' read as 'IN TTYS' -- a port is not an address.\n--- output ---\n${out}")
endif()

# ---- 3. examples/ai-mcp -- the machine an AI assistant debugs over --mcp. -----------------
#
# The README's first step, as the assistant takes it: boot (MCP does not run the startup, so
# `run from` 0), assemble HELLO.ASM, LOAD it, and run it -- which prints the bug the rest
# of the README goes on to find. `ELLO, WORLD` is the proof: the source on the disk, the
# assembler and the program all ran, and the bug is still there for the reader to fix. It runs
# on the COPY: ASM and LOAD write to the disk, and the tracked image must not move.
file(COPY "${SRC}/examples/ai-mcp" DESTINATION "${dist}/examples")
set(ai "${dist}/examples/ai-mcp")

mcp_session("${dist}/ai.jsonl"
  [=[{"name":"run","arguments":{"from":0,"until":"A>","timeout_ms":30000}}]=]
  [=[{"name":"run","arguments":{"input":"ASM HELLO\r","until":"END OF ASSEMBLY","timeout_ms":30000}}]=]
  [=[{"name":"run","arguments":{"until":"A>","timeout_ms":30000}}]=]
  [=[{"name":"run","arguments":{"input":"LOAD HELLO\r","until":"RECORDS WRITTEN","timeout_ms":30000}}]=]
  [=[{"name":"run","arguments":{"until":"A>","timeout_ms":30000}}]=]
  [=[{"name":"run","arguments":{"input":"HELLO\r","until":"WORLD","timeout_ms":30000}}]=])
execute_process(
  COMMAND           "${SIM}" cpm-ai.toml --mcp
  WORKING_DIRECTORY "${ai}"
  INPUT_FILE        "${dist}/ai.jsonl"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    out
  TIMEOUT           60
)
expect_contains("${out}" "the ai-mcp README's first step did not go as it says"
                "64K CP/M 2.2K" "END OF ASSEMBLY" "FIRST ADDRESS 0100" "ELLO, WORLD")

file(REMOVE_RECURSE "${dist}")
message(STATUS "examples: every machine file in the shipped examples works from its own folder.")
