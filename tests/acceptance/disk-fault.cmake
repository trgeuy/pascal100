# BAD SECTORS UNDER COMPUPRO'S OWN CP/M, on the Disk 1 and the Disk 1A (altairsim #966).
#
# The disks and the BIOSes are CompuPro's (docs/sources.md). Track 2, head 0, sector 1 is the
# first directory sector of both disks, so every DIR reads it. For each of the fault kinds that a
# read meets (data, id, missing):
#
#   1. count=2: the BIOS meets two errors and retries, and DIR lists the disk. The fault is used
#      twice and worn off.
#   2. hard: the BIOS tries ten times, and the BDOS says `Bdos Err On A: Bad Sector`.
#
# Then a hard `write` fault on that track: SAVE gives the same error, ten tries again.
#
# THE IMAGE MUST NOT CHANGE: a fault is an overlay on the drive. The test compares it byte for
# byte. The guest can write, so it works on a copy of tests/media/compupro.
#
# Expects: -DSIM=<pascal100> -DSRC=<source dir> -DBIN=<binary dir>

cmake_minimum_required(VERSION 3.16)

set(work "${BIN}/disk-fault-work")
file(REMOVE_RECURSE "${work}")
file(COPY "${SRC}/tests/media/compupro" DESTINATION "${work}")
set(dir "${work}/compupro")

# One tools/call line, appended to the session file, and the text that its reply must have. The
# call number is kept in a global property. These are functions: a macro would lose the
# backslashes of the JSON escapes.
function(call tool args want)
  get_property(n GLOBAL PROPERTY calls)
  math(EXPR n "${n} + 1")
  set_property(GLOBAL PROPERTY calls ${n})
  set_property(GLOBAL APPEND PROPERTY wants "${n}|${want}")
  file(APPEND "${work}/in.jsonl" "{\"jsonrpc\":\"2.0\",\"id\":${n},\"method\":\"tools/call\",\"params\":{\"name\":\"${tool}\",\"arguments\":${args}}}\n")
endfunction()
function(run text until want)
  call(run "{\"input\":\"${text}\",\"until\":\"${until}\",\"timeout_ms\":20000}" "${want}")
endfunction()
function(clear)
  call(disk_fault_clear "{\"id\":\"fd0\",\"unit\":\"drive0\",\"all\":true}" "no faults")
endfunction()
function(add fault)
  call(disk_fault_add "{\"id\":\"fd0\",\"unit\":\"drive0\",\"fault\":\"${fault}\"}" "fault ${fault}")
endfunction()
function(hits text)
  call(disk_fault_list "{}" "${text}")
endfunction()

foreach(pair "cpm22n.toml|CPM22N.DSK" "ccpm22ld.toml|CCPM22LD.DSK")
  string(REPLACE "|" ";" pair "${pair}")
  list(GET pair 0 machine)
  list(GET pair 1 image)
  file(SHA256 "${dir}/${image}" before)

  file(WRITE "${work}/in.jsonl" "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}\n{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n")
  set_property(GLOBAL PROPERTY calls 1)
  set_property(GLOBAL PROPERTY wants "")
  call(run "{\"from\":0,\"until\":\"A>\",\"timeout_ms\":20000}" "CP/M 2")
  foreach(kind data id missing)
    add("2/0/1 ${kind} count=2")
    run("DIR\\r" "\\r\\nA>" "COM")
    hits("hits 2, 0 left")
    clear()
    add("2/0/1 ${kind}")
    run("DIR\\r" "Bad Sector" "Bdos Err On A: Bad Sector")
    hits("hits 10")
    clear()
    run("\\u0003" "A>" "A>")
  endforeach()
  add("2/0/* write")
  run("SAVE 1 Q.COM\\r" "Bad Sector" "Bdos Err On A: Bad Sector")
  hits("hits 10")
  run("\\u0003" "A>" "A>")

  execute_process(COMMAND "${SIM}" "${machine}" --mcp
                  WORKING_DIRECTORY "${dir}"
                  INPUT_FILE "${work}/in.jsonl"
                  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc TIMEOUT 120)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "disk-fault: ${machine} exited ${rc}\n${out}\n${err}")
  endif()
  string(REPLACE "\r" "" out "${out}")
  get_property(wants GLOBAL PROPERTY wants)
  foreach(c IN LISTS wants)
    string(REGEX MATCH "^([0-9]+)\\|(.*)$" _ "${c}")
    set(id "${CMAKE_MATCH_1}")
    set(want "${CMAKE_MATCH_2}")
    string(REGEX MATCH "(^|\n){[^\n]*\"id\":${id}[,}][^\n]*" line "${out}")
    string(FIND "${line}" "${want}" at)
    if(at EQUAL -1 OR line MATCHES "\"isError\":true")
      message(FATAL_ERROR "disk-fault: ${machine}, call ${id}: no `${want}` in\n${line}")
    endif()
  endforeach()

  file(SHA256 "${dir}/${image}" after)
  if(NOT before STREQUAL after)
    message(FATAL_ERROR "disk-fault: ${image} changed. A fault must never touch the image.")
  endif()
endforeach()

file(REMOVE_RECURSE "${work}")
message(STATUS "disk-fault: CompuPro's CP/M retried past count=2 data, id and missing faults, gave "
               "Bad Sector after ten tries on hard ones, and the images did not change.")
