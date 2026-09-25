# Cross-platform checks for wasm2vircon's non-emitting inspection modes.
#
# Fixtures are selected by cli_inspection metadata roles, never by directory
# name. New, renamed, or removed cases therefore require no runner edits.

if(NOT DEFINED TOOL OR NOT DEFINED WASM_AS OR NOT DEFINED TESTS_DIR)
  message(FATAL_ERROR "cli-inspection-test.cmake requires TOOL, WASM_AS, and TESTS_DIR")
endif()

foreach(tool_path IN ITEMS "${TOOL}" "${WASM_AS}")
  if(NOT EXISTS "${tool_path}")
    message(FATAL_ERROR "required test tool does not exist: ${tool_path}")
  endif()
endforeach()

# Reads only fields used by this runner, leaving shared case metadata extensible.
function(read_cli_case case_directory prefix)
  set("${prefix}_source" "")
  set("${prefix}_entry" "main")
  set("${prefix}_role" "")
  set("${prefix}_error_expectations" "")
  set("${prefix}_profile_expectations" "")
  set("${prefix}_skip_error_expectations" "")
  file(STRINGS "${case_directory}/testcases" lines)
  foreach(line IN LISTS lines)
    if(line MATCHES "^[ \\t]*(#|$)")
      continue()
    endif()
    string(FIND "${line}" "=" separator)
    if(separator EQUAL -1)
      message(FATAL_ERROR "${case_directory}/testcases: expected key=value metadata")
    endif()
    string(SUBSTRING "${line}" 0 ${separator} key)
    math(EXPR value_start "${separator} + 1")
    string(SUBSTRING "${line}" ${value_start} -1 value)
    if(key STREQUAL "source")
      set("${prefix}_source" "${value}")
    elseif(key STREQUAL "entry")
      set("${prefix}_entry" "${value}")
    elseif(key STREQUAL "cli_inspection")
      set("${prefix}_role" "${value}")
    elseif(key STREQUAL "error_expectations")
      set("${prefix}_error_expectations" "${value}")
    elseif(key STREQUAL "cli_profile_expectations")
      set("${prefix}_profile_expectations" "${value}")
    elseif(key STREQUAL "cli_skip_error_expectations")
      set("${prefix}_skip_error_expectations" "${value}")
    endif()
  endforeach()
  foreach(field IN ITEMS source entry role error_expectations profile_expectations skip_error_expectations)
    set("${prefix}_${field}" "${${prefix}_${field}}" PARENT_SCOPE)
  endforeach()
endfunction()

# Finds all Wasm fixture directories that declare one CLI inspection role.
function(find_cli_cases role output_variable)
  file(GLOB case_directories LIST_DIRECTORIES true "${TESTS_DIR}/wat-*")
  list(SORT case_directories)
  set(matches "")
  foreach(case_directory IN LISTS case_directories)
    if(NOT EXISTS "${case_directory}/testcases")
      continue()
    endif()
    read_cli_case("${case_directory}" candidate)
    if(candidate_role STREQUAL "${role}")
      list(APPEND matches "${case_directory}")
    endif()
  endforeach()
  if(matches STREQUAL "")
    message(FATAL_ERROR "no wat-* test declares cli_inspection=${role}")
  endif()
  set("${output_variable}" "${matches}" PARENT_SCOPE)
endfunction()

# Requires a single fixture where an inspection behavior has one canonical case.
function(require_one_cli_case role output_variable)
  find_cli_cases("${role}" matches)
  list(LENGTH matches match_count)
  if(NOT match_count EQUAL 1)
    message(FATAL_ERROR "cli_inspection=${role} must identify exactly one fixture, found ${match_count}")
  endif()
  list(GET matches 0 match)
  set("${output_variable}" "${match}" PARENT_SCOPE)
endfunction()

# Assembles a declarative fixture and returns its entry name for invocation.
function(assemble_cli_fixture case_directory wasm_file prefix)
  read_cli_case("${case_directory}" fixture)
  if(fixture_source STREQUAL "")
    message(FATAL_ERROR "${case_directory}/testcases: cli fixture has no source")
  endif()
  set(source_path "${case_directory}/${fixture_source}")
  if(NOT EXISTS "${source_path}")
    message(FATAL_ERROR "cli fixture source not found: ${source_path}")
  endif()
  execute_process(COMMAND "${WASM_AS}" "${source_path}" -o "${wasm_file}"
    RESULT_VARIABLE result ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "wasm-as failed for ${source_path}: ${error}")
  endif()
  foreach(field IN ITEMS entry error_expectations profile_expectations skip_error_expectations)
    set("${prefix}_${field}" "${fixture_${field}}" PARENT_SCOPE)
  endforeach()
endfunction()

# Checks expectation files as literal, non-comment output fragments.
function(require_fragments context actual expectation_file)
  if(NOT EXISTS "${expectation_file}")
    message(FATAL_ERROR "${context}: missing expectations: ${expectation_file}")
  endif()
  file(READ "${actual}" actual_text)
  file(STRINGS "${expectation_file}" fragments)
  foreach(fragment IN LISTS fragments)
    if(fragment MATCHES "^[ \\t]*(#|$)")
      continue()
    endif()
    string(FIND "${actual_text}" "${fragment}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR "${context}: missing fragment: ${fragment}")
    endif()
  endforeach()
endfunction()

# Checks one explicit fragment without requiring a separate expectations file.
function(require_fragment context text fragment)
  string(FIND "${text}" "${fragment}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${context}: missing fragment: ${fragment}")
  endif()
endfunction()

string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef work_suffix)
set(work_dir "${CMAKE_CURRENT_BINARY_DIR}/wasm2vircon-cli-tests-${work_suffix}")
file(MAKE_DIRECTORY "${work_dir}")

require_one_cli_case(validate_accept valid_case)
assemble_cli_fixture("${valid_case}" "${work_dir}/valid.wasm" valid)
execute_process(
  COMMAND "${TOOL}" --validate-only "${work_dir}/valid.wasm" --entry "${valid_entry}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "--validate-only rejected cli_inspection=validate_accept: ${error}")
endif()
require_fragment("--validate-only" "${output}"
  "VirconWasm v1.14 validation passed for entry '${valid_entry}'")

require_one_cli_case(validate_reject rejected_case)
assemble_cli_fixture("${rejected_case}" "${work_dir}/rejected.wasm" rejected)
execute_process(
  COMMAND "${TOOL}" --validate-only "${work_dir}/rejected.wasm" --entry "${rejected_entry}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE rejected_error)
if(result EQUAL 0)
  message(FATAL_ERROR "--validate-only accepted cli_inspection=validate_reject unexpectedly")
endif()
if(rejected_error_expectations STREQUAL "")
  message(FATAL_ERROR "${rejected_case}/testcases: validate_reject requires error_expectations")
endif()
file(WRITE "${work_dir}/rejected.stderr" "${rejected_error}")
require_fragments("--validate-only rejection" "${work_dir}/rejected.stderr"
  "${rejected_case}/${rejected_error_expectations}")

execute_process(COMMAND "${TOOL}" --help RESULT_VARIABLE result OUTPUT_VARIABLE help ERROR_VARIABLE help_error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "--help failed: ${help_error}")
endif()
require_fragment("--help" "${help}" "Usage: wasm2vircon input.wasm")
string(FIND "${help}" "Embedded Binaryen normalization is enabled" embedded_normalizer)

find_cli_cases(legalizer legalizer_cases)
foreach(legalizer_case IN LISTS legalizer_cases)
  get_filename_component(legalizer_name "${legalizer_case}" NAME)
  assemble_cli_fixture("${legalizer_case}" "${work_dir}/${legalizer_name}.wasm" legalizer)
  foreach(extra_arguments IN ITEMS "" "--skip-input-optimization")
    execute_process(
      COMMAND "${TOOL}" --validate-only "${work_dir}/${legalizer_name}.wasm"
        --entry "${legalizer_entry}" ${extra_arguments}
      RESULT_VARIABLE result ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
      message(FATAL_ERROR "${legalizer_name}: linker-artifact legalization failed (${extra_arguments}): ${error}")
    endif()
  endforeach()
endforeach()

require_one_cli_case(embedded_optimizer optimizer_case)
assemble_cli_fixture("${optimizer_case}" "${work_dir}/optimizer.wasm" optimizer)
if(NOT embedded_normalizer EQUAL -1)
  execute_process(
    COMMAND "${TOOL}" --validate-only "${work_dir}/optimizer.wasm" --entry "${optimizer_entry}"
    RESULT_VARIABLE result ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "embedded optimizer rejected its declared fixture: ${error}")
  endif()
endif()
execute_process(
  COMMAND "${TOOL}" --validate-only "${work_dir}/optimizer.wasm" --entry "${optimizer_entry}"
    --skip-input-optimization
  RESULT_VARIABLE result ERROR_VARIABLE error)
if(result EQUAL 0)
  message(FATAL_ERROR "--skip-input-optimization accepted cli_inspection=embedded_optimizer")
endif()
if(optimizer_skip_error_expectations STREQUAL "")
  message(FATAL_ERROR "${optimizer_case}/testcases: embedded_optimizer requires cli_skip_error_expectations")
endif()
file(WRITE "${work_dir}/optimizer.stderr" "${error}")
require_fragments("unoptimized optimizer input" "${work_dir}/optimizer.stderr"
  "${optimizer_case}/${optimizer_skip_error_expectations}")

if(rejected_profile_expectations STREQUAL "")
  message(FATAL_ERROR "${rejected_case}/testcases: validate_reject requires cli_profile_expectations")
endif()
execute_process(
  COMMAND "${TOOL}" --report-profile "${work_dir}/rejected.wasm"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "--report-profile failed: ${error}")
endif()
file(WRITE "${work_dir}/profile.out" "${output}")
require_fragments("profile report" "${work_dir}/profile.out"
  "${rejected_case}/${rejected_profile_expectations}")

execute_process(
  COMMAND "${TOOL}" --report-profile "${work_dir}/valid.wasm" -o "${work_dir}/no.asm"
  RESULT_VARIABLE result ERROR_VARIABLE error)
if(result EQUAL 0)
  message(FATAL_ERROR "--report-profile accepted an output path unexpectedly")
endif()

file(REMOVE_RECURSE "${work_dir}")
