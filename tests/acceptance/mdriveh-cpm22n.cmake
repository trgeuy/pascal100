# THE COMPUPRO M-DRIVE/H ACCEPTANCE TEST: CompuPro's own CP/M 2.2N finds one M-Drive/H, formats
# it as drive M:, and reads and writes files on it.
#
# The disk is CompuPro's 2.2N system disk (tests/media/compupro, the disk of acceptance-disk1).
# Its CBIOS has the M-Drive/H driver built in: at cold boot it counts the boards with a read,
# write-back, read of each board's sector (HMDTEST, reference/CompuPro M-Drive H.md §6.3),
# prints "N M-Drive/H", and its auto command runs MFORM M. With no board it prints
# "0 M-Drive/H", and MFORM says the drive is not a memory drive.
#
# What it proves, through the real CLI over --mcp:
#   1. the sizing finds exactly one board: an absent board's range reads the floating bus;
#   2. MFORM formats the drive, and STAT reads the DPB the BIOS built (508K, 4 reserved tracks);
#   3. PIP [V] writes a file to M: and verifies it, and the copy RUNS from M:.
#
# The guest writes, so the test works on a copy of tests/media/compupro.
#
# Expects: -DSIM=<pascal100> -DSRC=<source dir> -DBIN=<binary dir>

cmake_minimum_required(VERSION 3.16)

set(work "${BIN}/mdriveh-work")
file(REMOVE_RECURSE "${work}")
file(COPY "${SRC}/tests/media/compupro" DESTINATION "${work}")
set(example "${work}/compupro")

# CP/M 2.2N's machine, plus one M-Drive/H at the CompuPro standard ports C6H/C7H.
file(WRITE "${example}/cpm22n-mdriveh.toml" "[machine]
name = \"cpm22n-mdriveh\"
base = \"cpm22n.toml\"

[[board]]
type = \"mdriveh\"
id   = \"mdh0\"
")

set(session "")
function(call id tool args)
  set(session "${session}{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tools/call\",\"params\":{\"name\":\"${tool}\",\"arguments\":${args}}}\n" PARENT_SCOPE)
endfunction()

# Type one command at A> and wait for the next A> at the start of a line.
function(cmd id text)
  call(${id} run "{\"input\":\"${text}\\r\",\"until\":\"\\r\\nA>\",\"timeout_ms\":30000}")
  set(session "${session}" PARENT_SCOPE)
endfunction()

set(session "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}\n{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n")
# MCP does not run the startup line: start the boot EPROM at 0000. The auto command MFORM M
# prints FORMATTING MEMORY DRIVE, then the CCP's prompt comes back.
call(2 run "{\"from\":0,\"until\":\"FORMATTING MEMORY DRIVE\",\"timeout_ms\":30000}")
call(3 run "{\"until\":\"\\r\\nA>\",\"timeout_ms\":30000}")
cmd(4 "STAT M:DSK:")
cmd(5 "PIP M:X.COM=STAT.COM[V]")
cmd(6 "M:X M:DSK:")
cmd(7 "DIR M:")
file(WRITE "${example}/in-mdriveh.jsonl" "${session}")

execute_process(
  COMMAND           "${SIM}" cpm22n-mdriveh.toml --mcp
  WORKING_DIRECTORY "${example}"
  INPUT_FILE        "${example}/in-mdriveh.jsonl"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    err
  RESULT_VARIABLE   rc
  TIMEOUT           110
)

function(fail why)
  message(FATAL_ERROR "mdriveh-cpm22n: ${why}\n--- stdout ---\n${out}\n--- stderr ---\n${err}")
endfunction()

if(NOT rc EQUAL 0)
  fail("`pascal100 cpm22n-mdriveh.toml --mcp` exited ${rc}, expected 0")
endif()

function(reply id var)
  string(REPLACE "\r" "" out_lf "${out}")
  string(REGEX MATCH "(^|\n){[^\n]*\"id\":${id}[,}][^\n]*" line "${out_lf}")
  set(${var} "${line}" PARENT_SCOPE)
endfunction()

foreach(id 2 3 4 5 6 7)
  reply(${id} r)
  if(NOT r MATCHES "\"stopped\":\"match\"")
    fail("run ${id} did not reach the text it waited for")
  endif()
endforeach()

# 1. The sizing found one board.
reply(2 r)
if(NOT r MATCHES "CompuPro Systems 64K CP/M 2\\.2N")
  fail("the boot did not print the 64K CP/M 2.2N sign-on")
endif()
if(NOT r MATCHES "1 M-Drive/H")
  fail("the BIOS did not count exactly one M-Drive/H")
endif()

# 2. The drive the BIOS built: one board = 508K of data, 8 sectors per track, 4 reserved tracks.
reply(4 r)
if(NOT r MATCHES "508: Kilobyte Drive  Capacity" OR NOT r MATCHES "4: Reserved Tracks")
  fail("STAT M:DSK: did not report the one-board M-Drive/H")
endif()

# 3. PIP's verify passed (no VERIFY ERROR), the copy runs from M:, and DIR lists it.
reply(5 r)
if(r MATCHES "VERIFY ERROR" OR r MATCHES "ERROR")
  fail("PIP [V] reported an error writing to M:")
endif()
reply(6 r)
if(NOT r MATCHES "508: Kilobyte Drive  Capacity")
  fail("M:X.COM, the copy of STAT.COM on the M-Drive/H, did not run")
endif()
reply(7 r)
if(NOT r MATCHES "M: X        COM")
  fail("DIR M: did not list X.COM")
endif()
