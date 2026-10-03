# End-to-end checks for DWARF-tolerant input and source-level diagnostics.

foreach(variable IN ITEMS TOOL WASM_AS ASSEMBLE FIXTURE_GENERATOR TESTS_DIR)
  if(NOT DEFINED ${variable})
    message(FATAL_ERROR "dwarf-input-test.cmake requires ${variable}")
  endif()
endforeach()

string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef suffix)
set(work "${CMAKE_CURRENT_BINARY_DIR}/wasm2vircon-dwarf-${suffix}")
file(MAKE_DIRECTORY "${work}")

# Generates one core module and an otherwise identical module carrying DWARF.
execute_process(COMMAND "${WASM_AS}" "${TESTS_DIR}/dwarf/accepted.wat" --debuginfo -o "${work}/plain.wasm"
  RESULT_VARIABLE result ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "cannot assemble DWARF base fixture: ${error}")
endif()
execute_process(COMMAND "${FIXTURE_GENERATOR}" "${work}/plain.wasm" "${work}/debug.wasm"
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "cannot append DWARF fixture sections")
endif()

execute_process(COMMAND "${TOOL}" --validate-only "${work}/debug.wasm" --entry main
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "DWARF-bearing module was rejected: ${error}")
endif()
string(FIND "${output}" "validation passed" found)
if(found EQUAL -1)
  message(FATAL_ERROR "DWARF validation did not report success")
endif()

execute_process(COMMAND "${TOOL}" "${work}/plain.wasm" --entry main -o "${work}/plain.asm"
  RESULT_VARIABLE result ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "plain fixture translation failed: ${error}")
endif()
execute_process(COMMAND "${TOOL}" "${work}/debug.wasm" --entry main -o "${work}/debug.asm"
  RESULT_VARIABLE result ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "DWARF fixture translation failed: ${error}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${work}/plain.asm" "${work}/debug.asm"
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "stripped and DWARF-bearing inputs generated different assembly")
endif()
execute_process(COMMAND "${ASSEMBLE}" -o "${work}/debug.vbin" "${work}/debug.asm"
  RESULT_VARIABLE result ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "DWARF fixture assembly failed: ${error}")
endif()

# The inspection report exposes sections and the recovered function source.
execute_process(COMMAND "${TOOL}" --report-profile "${work}/debug.wasm"
  RESULT_VARIABLE result OUTPUT_VARIABLE profile ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "DWARF profile report failed: ${error}")
endif()
foreach(fragment IN ITEMS
    "DWARF: 3 custom sections filtered before Binaryen"
    ".debug_info .debug_line reloc..debug_line"
    "source=fixture.zig:37:0")
  string(FIND "${profile}" "${fragment}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "DWARF profile report is missing: ${fragment}")
  endif()
endforeach()

# Unsupported VirconWasm operations include best-available source identity.
execute_process(COMMAND "${WASM_AS}" "${TESTS_DIR}/dwarf/unsupported.wat" --debuginfo
  -o "${work}/unsupported-plain.wasm" RESULT_VARIABLE result ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "cannot assemble unsupported DWARF fixture: ${error}")
endif()
execute_process(COMMAND "${FIXTURE_GENERATOR}" "${work}/unsupported-plain.wasm" "${work}/unsupported.wasm"
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "cannot append unsupported DWARF fixture sections")
endif()
execute_process(COMMAND "${TOOL}" --validate-only "${work}/unsupported.wasm" --entry main
  RESULT_VARIABLE result ERROR_VARIABLE unsupported_error)
if(result EQUAL 0)
  message(FATAL_ERROR "unsupported DWARF fixture was unexpectedly accepted")
endif()
foreach(fragment IN ITEMS "Wasm i32.clz" "function" "fixture.zig:37:0" "expression path")
  string(FIND "${unsupported_error}" "${fragment}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "source diagnostic is missing: ${fragment}\n${unsupported_error}")
  endif()
endforeach()

file(REMOVE_RECURSE "${work}")
