# THE COMPUPRO M-DRIVE/H, EIGHT BOARDS AND A RESET: CompuPro's own CP/M 2.2N counts all eight
# boards, and its files on drive M: live through a RESET and not through a POWER.
#
# What it proves, through the real CLI over --mcp (acceptance-mdriveh has the one-board drive):
#   1. the BIOS counts eight boards and builds the 2016K drive;
#   2. after a bus RESET and a cold boot, the BIOS does not format M: again, and the file is there
#      and runs;
#   3. after POWER it formats M:, and the drive is empty.
#
# The guest writes, so the test works on a copy of tests/media/compupro.
#
# Expects: -DSIM=<pascal100> -DSRC=<source dir> -DBIN=<binary dir>

cmake_minimum_required(VERSION 3.16)

set(work "${BIN}/mdriveh8-work")
file(REMOVE_RECURSE "${work}")
file(COPY "${SRC}/tests/media/compupro" DESTINATION "${work}")
set(example "${work}/compupro")

set(toml "[machine]\nname = \"cpm22n-mdriveh8\"\nbase = \"cpm22n.toml\"\n")
foreach(n RANGE 0 7)
  string(APPEND toml "\n[[board]]\ntype    = \"mdriveh\"\nid      = \"mdh${n}\"\ncascade = ${n}\n")
endforeach()
file(WRITE "${example}/cpm22n-mdriveh8.toml" "${toml}")

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
# The auto command is MFORM M. On a drive with no format it prints FORMATTING MEMORY DRIVE.
call(2 run "{\"from\":0,\"until\":\"FORMATTING MEMORY DRIVE\",\"timeout_ms\":30000}")
call(3 run "{\"until\":\"\\r\\nA>\",\"timeout_ms\":30000}")
cmd(4 "PIP M:X.COM=STAT.COM[V]")
cmd(5 "M:X M:DSK:")
call(6 monitor "{\"command\":\"RESET\"}")
call(7 run "{\"from\":0,\"until\":\"\\r\\nA>\",\"timeout_ms\":30000}")
cmd(8 "M:X M:DSK:")
call(9 monitor "{\"command\":\"POWER\"}")
call(10 run "{\"from\":0,\"until\":\"FORMATTING MEMORY DRIVE\",\"timeout_ms\":30000}")
call(11 run "{\"until\":\"\\r\\nA>\",\"timeout_ms\":30000}")
cmd(12 "DIR M:")
file(WRITE "${example}/in-mdriveh8.jsonl" "${session}")

execute_process(
  COMMAND           "${SIM}" cpm22n-mdriveh8.toml --mcp
  WORKING_DIRECTORY "${example}"
  INPUT_FILE        "${example}/in-mdriveh8.jsonl"
  OUTPUT_VARIABLE   out
  ERROR_VARIABLE    err
  RESULT_VARIABLE   rc
  TIMEOUT           110
)

function(fail why)
  message(FATAL_ERROR "mdriveh-boards: ${why}\n--- stdout ---\n${out}\n--- stderr ---\n${err}")
endfunction()

if(NOT rc EQUAL 0)
  fail("`pascal100 cpm22n-mdriveh8.toml --mcp` exited ${rc}, expected 0")
endif()

function(reply id var)
  string(REPLACE "\r" "" out_lf "${out}")
  string(REGEX MATCH "(^|\n){[^\n]*\"id\":${id}[,}][^\n]*" line "${out_lf}")
  set(${var} "${line}" PARENT_SCOPE)
endfunction()

foreach(id 2 3 4 5 7 8 10 11 12)
  reply(${id} r)
  if(NOT r MATCHES "\"stopped\":\"match\"")
    fail("run ${id} did not reach the text it waited for")
  endif()
endforeach()

# 1. Eight boards, and the drive the BIOS built from them. The copy on M: runs.
reply(2 r)
if(NOT r MATCHES "8 M-Drive/H")
  fail("the BIOS did not count eight M-Drive/H boards")
endif()
reply(4 r)
if(r MATCHES "ERROR")
  fail("PIP [V] reported an error writing to M:")
endif()
reply(5 r)
if(NOT r MATCHES "2016: Kilobyte Drive  Capacity")
  fail("STAT on M: did not report the 2016K drive of eight boards")
endif()

# 2. RESET keeps the boards and the files.
reply(7 r)
if(NOT r MATCHES "8 M-Drive/H" OR r MATCHES "FORMATTING")
  fail("after RESET the BIOS did not find its formatted eight-board drive")
endif()
reply(8 r)
if(NOT r MATCHES "2016: Kilobyte Drive  Capacity")
  fail("after RESET, M:X.COM did not run")
endif()

# 3. POWER loses them.
reply(10 r)
if(NOT r MATCHES "8 M-Drive/H")
  fail("after POWER the BIOS did not count eight boards")
endif()
reply(12 r)
if(NOT r MATCHES "NO FILE")
  fail("after POWER, M: was not empty")
endif()
