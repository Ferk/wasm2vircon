# Cross-platform runner for assembler-level VirconWasm profile tests.
#
# It deliberately consumes the existing per-case `testcases` metadata and
# handles only .wat inputs. The companion run-rom-tests.cmake script covers
# C-to-ROM/resource integration without invoking a shell driver.

if(NOT DEFINED TOOL OR NOT DEFINED WASM_AS OR NOT DEFINED ASSEMBLE OR NOT DEFINED TESTS_DIR)
  message(FATAL_ERROR "run-wasm-tests.cmake requires TOOL, WASM_AS, ASSEMBLE, and TESTS_DIR")
endif()

foreach(tool_path IN ITEMS "${TOOL}" "${WASM_AS}" "${ASSEMBLE}")
  if(NOT EXISTS "${tool_path}")
    message(FATAL_ERROR "required test tool does not exist: ${tool_path}")
  endif()
endforeach()

# Reads one key=value testcases file without imposing a second metadata format.
function(read_case_metadata case_file prefix)
  file(STRINGS "${case_file}" lines)
  foreach(line IN LISTS lines)
    if(line STREQUAL "" OR line MATCHES "^#")
      continue()
    endif()
    string(FIND "${line}" "=" separator)
    if(separator LESS 1)
      message(FATAL_ERROR "malformed test metadata in ${case_file}: ${line}")
    endif()
    string(SUBSTRING "${line}" 0 ${separator} key)
    math(EXPR value_start "${separator} + 1")
    string(SUBSTRING "${line}" ${value_start} -1 value)
    set("${prefix}_${key}" "${value}" PARENT_SCOPE)
  endforeach()
endfunction()

# Checks nonempty, non-comment expectation lines as literal file fragments.
function(require_fragments case_name expectation_file actual_file description)
  if(NOT EXISTS "${expectation_file}")
    message(FATAL_ERROR "${case_name}: missing ${description} expectations: ${expectation_file}")
  endif()
  file(READ "${actual_file}" actual)
  file(STRINGS "${expectation_file}" expected_lines)
  foreach(expected IN LISTS expected_lines)
    if(expected STREQUAL "" OR expected MATCHES "^#")
      continue()
    endif()
    string(FIND "${actual}" "${expected}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR "${case_name}: missing ${description} fragment: ${expected}")
    endif()
  endforeach()
endfunction()

# Runs one command and makes both streams available in an actionable failure.
function(run_checked case_name)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "${case_name}: command failed (${result})\nstdout:\n${output}\nstderr:\n${error}")
  endif()
endfunction()

string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef work_suffix)
set(work_dir "${CMAKE_CURRENT_BINARY_DIR}/wasm2vircon-wasm-tests-${work_suffix}")
file(MAKE_DIRECTORY "${work_dir}")

file(GLOB case_directories LIST_DIRECTORIES true "${TESTS_DIR}/wat-*")
list(SORT case_directories)
set(wat_case_count 0)

foreach(case_dir IN LISTS case_directories)
  if(NOT IS_DIRECTORY "${case_dir}" OR NOT EXISTS "${case_dir}/testcases")
    continue()
  endif()

  get_filename_component(case_name "${case_dir}" NAME)
  set(case_source "")
  set(case_entry "main")
  set(case_expectations "")
  set(case_error_expectations "")
  set(case_outcome "accept")
  set(case_wasm_as_options "")
  set(case_allow_stack_pointer "false")
  set(case_skip_input_optimization "false")
  set(case_cli_only "false")
  read_case_metadata("${case_dir}/testcases" case)

  if(case_source STREQUAL "")
    message(FATAL_ERROR "${case_name}: testcases has no source")
  endif()
  if(NOT case_source MATCHES "\\.wat$")
    continue()
  endif()
  if(case_cli_only STREQUAL "true")
    continue()
  elseif(NOT case_cli_only STREQUAL "false")
    message(FATAL_ERROR "${case_name}: cli_only must be true or false")
  endif()
  math(EXPR wat_case_count "${wat_case_count} + 1")

  set(source_path "${case_dir}/${case_source}")
  if(NOT EXISTS "${source_path}")
    message(FATAL_ERROR "${case_name}: missing source: ${source_path}")
  endif()
  get_filename_component(program_name "${case_source}" NAME_WE)
  set(case_output "${work_dir}/${case_name}")
  file(MAKE_DIRECTORY "${case_output}")
  set(wasm_file "${case_output}/${program_name}.wasm")
  set(asm_file "${case_output}/${program_name}.asm")

  set(wasm_as_arguments)
  if(NOT case_wasm_as_options STREQUAL "")
    separate_arguments(wasm_as_arguments NATIVE_COMMAND "${case_wasm_as_options}")
  endif()
  run_checked("${case_name}: wasm-as" "${WASM_AS}" ${wasm_as_arguments} "${source_path}" -o "${wasm_file}")

  set(compiler_arguments "${TOOL}" "${wasm_file}" --entry "${case_entry}")
  if(case_allow_stack_pointer STREQUAL "true")
    list(APPEND compiler_arguments --allow-stack-pointer)
  elseif(NOT case_allow_stack_pointer STREQUAL "false")
    message(FATAL_ERROR "${case_name}: allow_stack_pointer must be true or false")
  endif()
  if(case_skip_input_optimization STREQUAL "true")
    list(APPEND compiler_arguments --skip-input-optimization)
  elseif(NOT case_skip_input_optimization STREQUAL "false")
    message(FATAL_ERROR "${case_name}: skip_input_optimization must be true or false")
  endif()
  list(APPEND compiler_arguments -o "${asm_file}")

  if(case_outcome STREQUAL "accept")
    run_checked("${case_name}: wasm2vircon" ${compiler_arguments})
    run_checked("${case_name}: assemble" "${ASSEMBLE}" -o "${case_output}/${program_name}.vbin" "${asm_file}")
    if(NOT case_expectations STREQUAL "")
      require_fragments("${case_name}" "${case_dir}/${case_expectations}" "${asm_file}" "assembly")
    endif()
  elseif(case_outcome STREQUAL "reject")
    execute_process(
      COMMAND ${compiler_arguments}
      RESULT_VARIABLE result
      OUTPUT_VARIABLE output
      ERROR_VARIABLE error)
    if(result EQUAL 0)
      message(FATAL_ERROR "${case_name}: invalid Wasm was accepted")
    endif()
    if(NOT case_error_expectations STREQUAL "")
      set(error_file "${case_output}/${program_name}.stderr")
      file(WRITE "${error_file}" "${error}")
      require_fragments("${case_name}" "${case_dir}/${case_error_expectations}" "${error_file}" "diagnostic")
    endif()
  else()
    message(FATAL_ERROR "${case_name}: unsupported outcome for .wat input: ${case_outcome}")
  endif()
endforeach()

if(wat_case_count EQUAL 0)
  message(FATAL_ERROR "no .wat test cases found under ${TESTS_DIR}")
endif()

file(REMOVE_RECURSE "${work_dir}")
