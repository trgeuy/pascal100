# THE IEEE 696 ACCEPTANCE TEST (Stage 2, Task 3): CompuPro CP/M 2.2N boots and runs on the
# IEEE 696 bus type, from the two test machines in tests/media/ieee696:
#
#   cpm22n-alias64k.toml  64K of 8-bit memory that answers in every 64K segment
#   cpm22n-seg128k.toml   128K of 16-bit memory in segments 00 and 01 (set up like a RAM 23)
#
# Both are the Disk 1 machine of tests/media/compupro/cpm22n.toml. On
# the IEEE 696 bus the Z80 drives A8-A15 on every I/O cycle and the Disk 1 drives all 24 DMA address
# bits, so the boot, DIR, STAT and a PIP copy prove that every board on the machine still
# decodes its ports from A0-A7 and that the DMA reaches segment 00 through the wide path.
# tests/test_ieee696.cpp checks the same machines at the bus level.
#
# The guest writes (PIP), so the test works on copies, never the tracked images.
#
# Expects: -DSIM=<pascal100> -DSRC=<source dir> -DBIN=<binary dir>

cmake_minimum_required(VERSION 3.16)

set(work "${BIN}/ieee696-work")
file(REMOVE_RECURSE "${work}")
file(COPY "${SRC}/tests/media/compupro" DESTINATION "${work}")
file(COPY "${SRC}/tests/media/ieee696" DESTINATION "${work}")

set(session "")
function(call id tool args)
  set(session "${session}{\"jsonrpc\":\"2.0\",\"id\":${id},\"method\":\"tools/call\",\"params\":{\"name\":\"${tool}\",\"arguments\":${args}}}\n" PARENT_SCOPE)
endfunction()
function(cmd id text)
  call(${id} run "{\"input\":\"${text}\\r\",\"until\":\"\\r\\nA>\",\"timeout_ms\":20000}")
  set(session "${session}" PARENT_SCOPE)
endfunction()

foreach(machine cpm22n-alias64k cpm22n-seg128k)
  # A fresh copy of the disk for each machine, so the first one's PIP does not matter.
  file(COPY "${SRC}/tests/media/compupro/CPM22N.DSK" DESTINATION "${work}/compupro")

  set(session "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}\n{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n")
  # MCP does not run the machine file's startup, so start the boot EPROM at 0000 here.
  call(2 run "{\"from\":0,\"until\":\"A>\",\"timeout_ms\":20000}")
  cmd(3 "DIR")
  cmd(4 "STAT DSK:")
  cmd(5 "PIP X.COM=STAT.COM")
  cmd(6 "X DSK:")
  file(WRITE "${work}/ieee696/in-${machine}.jsonl" "${session}")

  execute_process(
    COMMAND           "${SIM}" ${machine}.toml --mcp
    WORKING_DIRECTORY "${work}/ieee696"
    INPUT_FILE        "${work}/ieee696/in-${machine}.jsonl"
    OUTPUT_VARIABLE   out
    ERROR_VARIABLE    err
    RESULT_VARIABLE   rc
    TIMEOUT           60
  )

  function(fail why)
    message(FATAL_ERROR "ieee696 (${machine}): ${why}\n--- stdout ---\n${out}\n--- stderr ---\n${err}")
  endfunction()
  function(reply id var)
    string(REPLACE "\r" "" out_lf "${out}")
    string(REGEX MATCH "(^|\n){[^\n]*\"id\":${id}[,}][^\n]*" line "${out_lf}")
    set(${var} "${line}" PARENT_SCOPE)
  endfunction()

  if(NOT rc EQUAL 0)
    fail("`pascal100 ${machine}.toml --mcp` exited ${rc}, expected 0")
  endif()
  foreach(id 2 3 4 5 6)
    reply(${id} r)
    if(NOT r MATCHES "\"stopped\":\"match\"")
      fail("run ${id} did not come back to A>")
    endif()
  endforeach()
  reply(2 r)
  if(NOT r MATCHES "CompuPro Systems 64K CP/M 2\\.2N")
    fail("the boot did not print the 64K CP/M 2.2N sign-on")
  endif()
  reply(3 r)
  if(NOT r MATCHES "HMXFPROM ASM" OR NOT r MATCHES "SYSGEN   COM")
    fail("DIR did not list HMXFPROM.ASM and SYSGEN.COM")
  endif()
  reply(4 r)
  if(NOT r MATCHES "600: Kilobyte Drive  Capacity")
    fail("STAT DSK: did not report the 600K drive")
  endif()
  reply(6 r)
  if(NOT r MATCHES "600: Kilobyte Drive  Capacity")
    fail("X.COM, the copy of STAT.COM that PIP wrote, did not run")
  endif()
  message(STATUS "ieee696: ${machine} boots CP/M 2.2N, DIR, STAT, PIP -- ok")
endforeach()
