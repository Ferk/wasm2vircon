# Collects comparable code-generation metrics from examples and a small workload.
#
# This is an opt-in developer tool, not part of the normal build or CTest. Each
# language's native example build produces Wasm; one explicitly selected
# wasm2vircon executable then translates every available module so the backend
# measurements are directly comparable.

cmake_minimum_required(VERSION 3.20)

get_filename_component(REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

if(NOT DEFINED OUTPUT_DIR)
  set(OUTPUT_DIR "${REPOSITORY_ROOT}/build/optimization-baseline")
endif()
get_filename_component(OUTPUT_DIR "${OUTPUT_DIR}" ABSOLUTE)

if(NOT DEFINED BUILD_EXAMPLES)
  set(BUILD_EXAMPLES ON)
endif()
if(NOT DEFINED REQUIRE_ALL)
  set(REQUIRE_ALL OFF)
endif()
if(NOT DEFINED ENABLE_V32SIM)
  set(ENABLE_V32SIM ON)
endif()
if(NOT DEFINED EXAMPLES OR EXAMPLES STREQUAL "")
  set(EXAMPLES c-boids cpp-boids zig-boids rust-boids tinygo-boids)
endif()

set(wasm2vircon_was_explicit FALSE)
if(DEFINED WASM2VIRCON_EXECUTABLE AND NOT WASM2VIRCON_EXECUTABLE STREQUAL "")
  set(wasm2vircon_was_explicit TRUE)
else()
  unset(WASM2VIRCON_EXECUTABLE)
  unset(WASM2VIRCON_EXECUTABLE CACHE)
  find_program(WASM2VIRCON_EXECUTABLE NAMES wasm2vircon)
  if(NOT WASM2VIRCON_EXECUTABLE)
    unset(WASM2VIRCON_EXECUTABLE)
    unset(WASM2VIRCON_EXECUTABLE CACHE)
    find_program(WASM2VIRCON_EXECUTABLE NAMES wasm2vircon wasm2vircon.exe
      HINTS "${REPOSITORY_ROOT}/build" "${REPOSITORY_ROOT}"
      NO_DEFAULT_PATH)
  endif()
endif()
if(NOT WASM2VIRCON_EXECUTABLE OR NOT EXISTS "${WASM2VIRCON_EXECUTABLE}")
  if(wasm2vircon_was_explicit)
    message(FATAL_ERROR
      "The explicitly selected wasm2vircon executable does not exist: ${WASM2VIRCON_EXECUTABLE}. "
      "Replace documentation placeholders such as /absolute/path/to/wasm2vircon with a real path.")
  else()
    message(FATAL_ERROR
      "wasm2vircon was not found on PATH or at build/wasm2vircon. Build the project first, install it on PATH, "
      "or pass -DWASM2VIRCON_EXECUTABLE=/real/path/to/wasm2vircon")
  endif()
endif()

find_program(ASSEMBLE_EXECUTABLE NAMES assemble REQUIRED)
find_program(PACKROM_EXECUTABLE NAMES packrom)
find_program(GIT_EXECUTABLE NAMES git)
find_package(Python3 QUIET COMPONENTS Interpreter)
if(ENABLE_V32SIM)
  find_program(V32SIM_EXECUTABLE NAMES v32sim)
  find_file(V32SIM_BIOS NAMES BiosWithoutLogo.v32
    HINTS /usr/local/Vircon32/Emulator/Bios /usr/local/share/vircon32)
endif()

set(known_examples c-boids cpp-boids zig-boids rust-boids tinygo-boids)
foreach(example IN LISTS EXAMPLES)
  if(NOT example IN_LIST known_examples)
    message(FATAL_ERROR "unknown optimization-baseline example: ${example}")
  endif()
endforeach()

# The work directory belongs exclusively to this script. Clearing it ensures
# frontend outputs cannot remain stale across compiler/backend measurements.
if(BUILD_EXAMPLES)
  file(REMOVE_RECURSE "${OUTPUT_DIR}/work")
endif()
file(REMOVE_RECURSE "${OUTPUT_DIR}/artifacts")
file(MAKE_DIRECTORY "${OUTPUT_DIR}/work" "${OUTPUT_DIR}/artifacts")

set(SUMMARY_CSV "${OUTPUT_DIR}/summary.csv")
set(FRAMES_CSV "${OUTPUT_DIR}/frames.csv")
set(TOUR_CSV "${OUTPUT_DIR}/tour-results.csv")
set(REPORT_MD "${OUTPUT_DIR}/report.md")
set(ENVIRONMENT_TXT "${OUTPUT_DIR}/environment.txt")

file(WRITE "${SUMMARY_CSV}"
  "example,language,wasm_bytes,assembly_bytes,instruction_count,program_words,"
  "frame_count,max_frame_words,wasm_sha256,assembly_sha256,vbin_sha256,runtime_metric\n")
file(WRITE "${FRAMES_CSV}" "example,function_label,frame_words\n")
file(WRITE "${TOUR_CSV}" "example,case,count,fps,gpu_percent,cpu_percent\n")
file(WRITE "${REPORT_MD}" "# wasm2vircon optimization baseline\n\n")
file(APPEND "${REPORT_MD}"
  "All available example modules in this report were translated with the same wasm2vircon binary. "
  "Program words come from the assembled VBIN size; instruction count excludes labels, comments, and data "
  "directives.\n\n"
  "| Example | Language | Instructions | Program words | Frames | Maximum frame | Runtime metric |\n"
  "| --- | --- | ---: | ---: | ---: | ---: | --- |\n")

# Records the first output line of one tool's version command.
function(record_version label program)
  if(NOT program OR NOT EXISTS "${program}")
    file(APPEND "${ENVIRONMENT_TXT}" "${label}=unavailable\n")
    return()
  endif()
  execute_process(
    COMMAND "${program}" ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)
  if(NOT result EQUAL 0)
    file(APPEND "${ENVIRONMENT_TXT}" "${label}=${program} (version command failed)\n")
    return()
  endif()
  if(output STREQUAL "")
    set(output "${error}")
  endif()
  string(REPLACE "\r\n" "\n" output "${output}")
  string(REPLACE "\r" "\n" output "${output}")
  string(REGEX REPLACE "\n.*" "" first_line "${output}")
  file(APPEND "${ENVIRONMENT_TXT}" "${label}=${first_line}\n")
endfunction()

file(SHA256 "${WASM2VIRCON_EXECUTABLE}" wasm2vircon_sha256)
file(SHA256 "${ASSEMBLE_EXECUTABLE}" assemble_sha256)
file(WRITE "${ENVIRONMENT_TXT}"
  "wasm2vircon.path=${WASM2VIRCON_EXECUTABLE}\n"
  "wasm2vircon.sha256=${wasm2vircon_sha256}\n"
  "assemble.path=${ASSEMBLE_EXECUTABLE}\n"
  "assemble.sha256=${assemble_sha256}\n")
if(GIT_EXECUTABLE)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
    WORKING_DIRECTORY "${REPOSITORY_ROOT}"
    RESULT_VARIABLE git_result
    OUTPUT_VARIABLE git_revision
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(git_result EQUAL 0)
    file(APPEND "${ENVIRONMENT_TXT}" "repository.revision=${git_revision}\n")
  endif()
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" status --porcelain
    WORKING_DIRECTORY "${REPOSITORY_ROOT}"
    OUTPUT_VARIABLE git_status
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(git_status STREQUAL "")
    file(APPEND "${ENVIRONMENT_TXT}" "repository.dirty=no\n")
  else()
    file(APPEND "${ENVIRONMENT_TXT}" "repository.dirty=yes\n")
  endif()
endif()
record_version("v32sim" "${V32SIM_EXECUTABLE}" --version)
if(V32SIM_EXECUTABLE AND EXISTS "${V32SIM_EXECUTABLE}")
  file(SHA256 "${V32SIM_EXECUTABLE}" v32sim_sha256)
  file(APPEND "${ENVIRONMENT_TXT}" "v32sim.path=${V32SIM_EXECUTABLE}\nv32sim.sha256=${v32sim_sha256}\n")
endif()
if(V32SIM_BIOS AND EXISTS "${V32SIM_BIOS}")
  file(SHA256 "${V32SIM_BIOS}" bios_sha256)
  file(APPEND "${ENVIRONMENT_TXT}" "v32sim.bios.path=${V32SIM_BIOS}\nv32sim.bios.sha256=${bios_sha256}\n")
endif()

# Reports a missing optional frontend or turns it into a strict-run failure.
function(skip_example example reason)
  if(REQUIRE_ALL)
    message(FATAL_ERROR "${example}: ${reason}")
  endif()
  message(WARNING "${example}: ${reason}; skipping")
  file(APPEND "${REPORT_MD}" "| ${example} | unavailable | - | - | - | - | ${reason} |\n")
  set_property(GLOBAL APPEND PROPERTY BENCHMARK_SKIPPED "${example}: ${reason}")
endfunction()

# Runs a required build/translation command with captured actionable output.
function(run_checked description)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "${description} failed (${result})\nstdout:\n${output}\nstderr:\n${error}")
  endif()
endfunction()

# Extracts instruction, VBIN-word, and stack-frame metrics for one example.
function(collect_metrics example language wasm_file asm_file vbin_file runtime_metric)
  file(SIZE "${wasm_file}" wasm_bytes)
  file(SIZE "${asm_file}" assembly_bytes)
  file(SIZE "${vbin_file}" vbin_bytes)
  file(READ "${vbin_file}" vbin_signature OFFSET 0 LIMIT 8 HEX)
  string(TOLOWER "${vbin_signature}" vbin_signature)
  if(NOT vbin_signature STREQUAL "5633322d5642494e")
    message(FATAL_ERROR "${example}: ${vbin_file} does not have the V32-VBIN signature")
  endif()
  math(EXPR vbin_payload "${vbin_bytes} - 12")
  math(EXPR vbin_remainder "${vbin_payload} % 4")
  if(vbin_payload LESS 4 OR NOT vbin_remainder EQUAL 0)
    message(FATAL_ERROR "${example}: ${vbin_file} is not a structurally valid VBIN")
  endif()
  math(EXPR payload_words "${vbin_payload} / 4")
  file(READ "${vbin_file}" vbin_word_bytes OFFSET 8 LIMIT 4 HEX)
  string(SUBSTRING "${vbin_word_bytes}" 0 2 word_byte_0)
  string(SUBSTRING "${vbin_word_bytes}" 2 2 word_byte_1)
  string(SUBSTRING "${vbin_word_bytes}" 4 2 word_byte_2)
  string(SUBSTRING "${vbin_word_bytes}" 6 2 word_byte_3)
  math(EXPR program_words "0x${word_byte_3}${word_byte_2}${word_byte_1}${word_byte_0}")
  if(NOT program_words EQUAL payload_words)
    message(FATAL_ERROR
      "${example}: VBIN declares ${program_words} words but its payload contains ${payload_words}")
  endif()

  file(SHA256 "${wasm_file}" wasm_sha256)
  file(SHA256 "${asm_file}" assembly_sha256)
  file(SHA256 "${vbin_file}" vbin_sha256)
  file(STRINGS "${asm_file}" assembly_lines)

  set(instruction_count 0)
  set(frame_count 0)
  set(max_frame_words 0)
  set(frame_label "")
  set(prologue_stage 0)
  foreach(line IN LISTS assembly_lines)
    if(line MATCHES "^[ \t]+([A-Za-z][A-Za-z0-9_]*)($|[ \t])")
      string(TOLOWER "${CMAKE_MATCH_1}" opcode)
      if(NOT opcode STREQUAL "integer" AND NOT opcode STREQUAL "float" AND
         NOT opcode STREQUAL "pointer" AND NOT opcode STREQUAL "string" AND
         NOT opcode STREQUAL "datafile")
        math(EXPR instruction_count "${instruction_count} + 1")
      endif()
    endif()

    string(STRIP "${line}" stripped)
    if(stripped STREQUAL "" OR stripped MATCHES "^;")
      continue()
    endif()
    if(stripped MATCHES "^([A-Za-z_][A-Za-z0-9_]*):$")
      set(frame_label "${CMAKE_MATCH_1}")
      set(prologue_stage 1)
    elseif(prologue_stage EQUAL 1 AND stripped STREQUAL "push BP")
      set(prologue_stage 2)
    elseif(prologue_stage EQUAL 2 AND stripped STREQUAL "mov BP, SP")
      set(prologue_stage 3)
    elseif(prologue_stage EQUAL 3 AND stripped MATCHES "^isub SP, ([0-9]+)$")
      set(frame_words "${CMAKE_MATCH_1}")
      math(EXPR frame_count "${frame_count} + 1")
      if(frame_words GREATER max_frame_words)
        set(max_frame_words "${frame_words}")
      endif()
      file(APPEND "${FRAMES_CSV}" "${example},${frame_label},${frame_words}\n")
      set(prologue_stage 0)
    else()
      set(prologue_stage 0)
    endif()
  endforeach()

  file(APPEND "${SUMMARY_CSV}"
    "${example},${language},${wasm_bytes},${assembly_bytes},${instruction_count},${program_words},"
    "${frame_count},${max_frame_words},${wasm_sha256},${assembly_sha256},${vbin_sha256},${runtime_metric}\n")
  file(APPEND "${REPORT_MD}"
    "| ${example} | ${language} | ${instruction_count} | ${program_words} | ${frame_count} | "
    "${max_frame_words} | ${runtime_metric} |\n")
  set_property(GLOBAL APPEND PROPERTY BENCHMARK_ROWS
    "${example}|${language}|${instruction_count}|${program_words}|${max_frame_words}")
  if(runtime_metric MATCHES "tour_instructions=([0-9]+);tour_display_frames=([0-9]+)")
    set_property(GLOBAL APPEND PROPERTY BENCHMARK_TOURS "${example}|${CMAKE_MATCH_1}|${CMAKE_MATCH_2}")
  endif()
endfunction()

# Pad one column, preserving longer values instead of truncating them.
function(pad_column output value width alignment)
  string(LENGTH "${value}" length)
  math(EXPR padding "${width} - ${length}")
  if(padding LESS 0)
    set(padding 0)
  endif()
  string(REPEAT " " ${padding} spaces)
  if(alignment STREQUAL "right")
    set(${output} "${spaces}${value}" PARENT_SCOPE)
  else()
    set(${output} "${value}${spaces}" PARENT_SCOPE)
  endif()
endfunction()

# Print the collected metrics without requiring users to open the report.
function(print_benchmark_summary)
  get_property(rows GLOBAL PROPERTY BENCHMARK_ROWS)
  set(summary "\nBenchmark results\n")
  string(APPEND summary "  Example       Language   ASM instructions   Program words   Max frame (words)\n")
  string(APPEND summary "  ------------  --------  -----------------  -------------  -----------------\n")
  foreach(row IN LISTS rows)
    string(REPLACE "|" ";" columns "${row}")
    list(GET columns 0 example)
    list(GET columns 1 language)
    list(GET columns 2 instructions)
    list(GET columns 3 words)
    list(GET columns 4 frame)
    pad_column(example_col "${example}" 12 left)
    pad_column(language_col "${language}" 8 left)
    pad_column(instructions_col "${instructions}" 17 right)
    pad_column(words_col "${words}" 13 right)
    pad_column(frame_col "${frame}" 17 right)
    string(APPEND summary "  ${example_col}  ${language_col}  ${instructions_col}  ${words_col}  ${frame_col}\n")
  endforeach()
  get_property(tours GLOBAL PROPERTY BENCHMARK_TOURS)
  foreach(tour IN LISTS tours)
    string(REPLACE "|" ";" tour_columns "${tour}")
    list(GET tour_columns 0 tour_example)
    list(GET tour_columns 1 tour_instructions)
    list(GET tour_columns 2 tour_frames)
    string(APPEND summary "\n${tour_example} tour (v32sim): ${tour_instructions} instructions, ${tour_frames} display frames\n")
  endforeach()
  if(tours)
    string(APPEND summary "  Tour profiles include BIOS startup and all six cases; not GPU timings.\n")
  else()
    string(APPEND summary "\nBenchmark tour profiles unavailable or disabled.\n")
  endif()
  get_property(skipped GLOBAL PROPERTY BENCHMARK_SKIPPED)
  foreach(reason IN LISTS skipped)
    string(APPEND summary "  Skipped ${reason}\n")
  endforeach()
  string(APPEND summary "\nResults: ${OUTPUT_DIR}\n  report.md  summary.csv  frames.csv  artifacts/\n")
  message("${summary}")
endfunction()

# Run the complete Start-button tour and retain its aggregate address profile.
function(profile_benchmarks example rom_file output_metric)
  set(${output_metric} "not_available" PARENT_SCOPE)
  if(NOT ENABLE_V32SIM)
    message(STATUS "Benchmark instruction profile disabled")
    return()
  endif()
  if(NOT V32SIM_EXECUTABLE OR NOT EXISTS "${V32SIM_EXECUTABLE}" OR NOT V32SIM_BIOS OR NOT EXISTS "${V32SIM_BIOS}")
    message(STATUS "Benchmark instruction profile unavailable: set V32SIM_EXECUTABLE and V32SIM_BIOS")
    return()
  endif()
  execute_process(COMMAND "${V32SIM_EXECUTABLE}" --help RESULT_VARIABLE help_status OUTPUT_VARIABLE help)
  if(NOT help_status EQUAL 0 OR NOT help MATCHES "--batch" OR NOT help MATCHES "--profile-out" OR
     NOT help MATCHES "--fixed-clock" OR NOT help MATCHES "--fast" OR NOT help MATCHES "--seed" OR
     NOT help MATCHES "--exit-status" OR NOT help MATCHES "--max-frames")
    message(WARNING "Benchmark instruction profiles require a recent v32sim with --profile-out and deterministic options")
    return()
  endif()
  set(profile "${OUTPUT_DIR}/artifacts/${example}-tour.profile")
  set(simulation_log "${OUTPUT_DIR}/artifacts/${example}-tour.v32sim.out")
  if(Python3_Interpreter_FOUND)
    set(case_results "${OUTPUT_DIR}/artifacts/${example}-tour.csv")
    execute_process(
      COMMAND "${Python3_EXECUTABLE}" -u "${CMAKE_CURRENT_LIST_DIR}/benchmark-tour.py"
        "${V32SIM_EXECUTABLE}" "${V32SIM_BIOS}" "${rom_file}" "${profile}" "${simulation_log}"
        --label "${example}" --results "${case_results}"
      RESULT_VARIABLE result TIMEOUT 180)
    if(NOT result STREQUAL "0" OR NOT EXISTS "${profile}" OR NOT EXISTS "${case_results}")
      message(FATAL_ERROR "Benchmark tour simulation failed (${result}); see ${simulation_log}")
    endif()
    file(STRINGS "${case_results}" case_lines)
    list(REMOVE_AT case_lines 0)
    set(case_signature "")
    foreach(case_line IN LISTS case_lines)
      if(NOT case_line MATCHES "^([^,]+),([0-9]+),")
        message(FATAL_ERROR "${example}: invalid benchmark result: ${case_line}")
      endif()
      string(APPEND case_signature "${CMAKE_MATCH_1}:${CMAKE_MATCH_2}|")
      file(APPEND "${TOUR_CSV}" "${example},${case_line}\n")
      string(REPLACE "," "|" case_columns "${case_line}")
      set_property(GLOBAL APPEND PROPERTY BENCHMARK_CASE_ROWS "${example}|${case_columns}")
    endforeach()
    get_property(reference_signature GLOBAL PROPERTY BENCHMARK_CASE_SIGNATURE)
    if(reference_signature AND NOT case_signature STREQUAL reference_signature)
      message(FATAL_ERROR "${example}: tour cases differ from the first benchmark; results are not comparable")
    endif()
    set_property(GLOBAL PROPERTY BENCHMARK_CASE_SIGNATURE "${case_signature}")
  else()
    message(STATUS "Python 3 is unavailable; collecting the aggregate profile without live case results")
    set(commands "${OUTPUT_DIR}/artifacts/${example}-tour.commands")
    file(WRITE "${commands}" "gamepad0 connect\ngamepad0 start\nwatch/d GPU_DrawingPointY >= 1001\n")
    foreach(case_index RANGE 1 6)
      file(APPEND "${commands}" "continue\n")
    endforeach()
    file(APPEND "${commands}" "watch GPU_DrawingPointY == 6\ncontinue\nprint/d GPU_DrawingPointX\nprint/d GPU_DrawingPointY\nquit\n")
    execute_process(
      COMMAND "${V32SIM_EXECUTABLE}" --batch --no-debug --errorcheck --exit-status --fast
        --fixed-clock --seed=1 --max-frames=10000
        --biosfile "${V32SIM_BIOS}" --entry-point 0x10000004
        "--profile-out=${profile}" "${rom_file}"
      INPUT_FILE "${commands}"
      RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 180)
    file(WRITE "${simulation_log}" "${output}${error}")
    if(NOT result STREQUAL "0" OR NOT output MATCHES "0x207/d:[ \t]*[0-9]+([^0-9]|$)" OR
       NOT output MATCHES "0x208/d:[ \t]*6([^0-9]|$)" OR NOT EXISTS "${profile}")
      message(FATAL_ERROR "Benchmark tour simulation failed (${result}):\n${output}\n${error}")
    endif()
  endif()
  file(STRINGS "${profile}" profile_header REGEX "^# cycles [0-9]+ frames [0-9]+$")
  if(NOT profile_header MATCHES "^# cycles ([0-9]+) frames ([0-9]+)$")
    message(FATAL_ERROR "Benchmark tour profile has an unexpected header")
  endif()
  set(${output_metric} "tour_instructions=${CMAKE_MATCH_1};tour_display_frames=${CMAKE_MATCH_2}" PARENT_SCOPE)
endfunction()

set(collected_count 0)
foreach(example IN LISTS EXAMPLES)
  set(example_dir "${REPOSITORY_ROOT}/examples/${example}")
  set(example_build_dir "${OUTPUT_DIR}/work/${example}")
  set(language "")
  set(entry "vircon_main")

  if(example STREQUAL "c-boids")
    set(language "C")
    set(entry "main")
    set(project_file "CBoids")
  elseif(example STREQUAL "cpp-boids")
    set(language "C++")
    set(project_file "CppBoids")
  elseif(example STREQUAL "rust-boids")
    set(language "Rust")
    set(project_file "RustBoids")
  elseif(example STREQUAL "tinygo-boids")
    set(language "TinyGo")
    set(project_file "TinyGoBoids")
  elseif(example STREQUAL "zig-boids")
    set(language "Zig")
    set(project_file "zig-boids")
  endif()

  if(BUILD_EXAMPLES)
    if(example STREQUAL "zig-boids")
      execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
          "ZIG_GLOBAL_CACHE_DIR=${example_build_dir}/zig-global-cache"
          "ZIG_LOCAL_CACHE_DIR=${example_build_dir}/zig-cache"
          zig build wasm --cache-dir "${example_build_dir}/zig-cache"
          --global-cache-dir "${example_build_dir}/zig-global-cache"
        WORKING_DIRECTORY "${example_dir}"
        RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
      string(STRIP "${build_error}" build_error_trimmed)
      string(TOLOWER "${build_error_trimmed}" build_error_lower)
      if(NOT build_result STREQUAL "0" AND build_error_lower MATCHES "^(no such file or directory|the system cannot find the file specified)\\.?$")
        skip_example("${example}" "could not start Zig: ${build_error_trimmed}")
        continue()
      elseif(NOT build_result STREQUAL "0")
        message(FATAL_ERROR "${example} Wasm build failed (${build_result})\nstdout:\n${build_output}\nstderr:\n${build_error}")
      endif()
      set(wasm_file "${example_dir}/build/${project_file}.wasm")
    elseif(example STREQUAL "rust-boids")
      # Delegate the frontend build to the example's Cargo task. Its target
      # directory is isolated so this run cannot reuse stale Wasm output.
      set(rust_target "${example_build_dir}/cargo-target")
      execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "CARGO_TARGET_DIR=${rust_target}"
          cargo v32 --wasm-only
        WORKING_DIRECTORY "${example_dir}"
        RESULT_VARIABLE build_result OUTPUT_VARIABLE build_output ERROR_VARIABLE build_error)
      string(STRIP "${build_error}" build_error_trimmed)
      string(TOLOWER "${build_error_trimmed}" build_error_lower)
      if(NOT build_result STREQUAL "0" AND build_error_lower MATCHES "^(no such file or directory|the system cannot find the file specified)\\.?$")
        skip_example("${example}" "could not start Cargo: ${build_error_trimmed}")
        continue()
      elseif(NOT build_result STREQUAL "0")
        message(FATAL_ERROR "${example} Wasm build failed (${build_result})\nstdout:\n${build_output}\nstderr:\n${build_error}")
      endif()
      set(wasm_file "${rust_target}/wasm32-unknown-unknown/release/rust-boids.wasm")
    else()
      execute_process(
        COMMAND "${CMAKE_COMMAND}" -S "${example_dir}" -B "${example_build_dir}"
          "-DWASM2VIRCON_EXECUTABLE=${WASM2VIRCON_EXECUTABLE}"
        RESULT_VARIABLE configure_result OUTPUT_VARIABLE configure_output ERROR_VARIABLE configure_error)
      if(NOT configure_result STREQUAL "0")
        if(configure_error MATCHES "Could not find [A-Za-z0-9_]+[ \t\r\n]+using the following names")
          skip_example("${example}" "${CMAKE_MATCH_0}")
          continue()
        endif()
        message(FATAL_ERROR "${example} configure failed (${configure_result})\nstdout:\n${configure_output}\nstderr:\n${configure_error}")
      endif()
      run_checked("${example} ROM build"
        "${CMAKE_COMMAND}" --build "${example_build_dir}" --target v32)
      set(wasm_file "${example_build_dir}/${project_file}.wasm")
    endif()
  else()
    if(example STREQUAL "rust-boids")
      if(DEFINED ENV{CARGO_TARGET_DIR} AND NOT "$ENV{CARGO_TARGET_DIR}" STREQUAL "")
        get_filename_component(rust_target "$ENV{CARGO_TARGET_DIR}" ABSOLUTE BASE_DIR "${example_dir}")
      else()
        set(rust_target "${example_dir}/target")
      endif()
      set(wasm_file "${rust_target}/wasm32-unknown-unknown/release/rust-boids.wasm")
    else()
      set(wasm_file "${example_dir}/build/${project_file}.wasm")
    endif()
    if(NOT EXISTS "${wasm_file}")
      file(GLOB existing_wasm_files "${example_dir}/build/*.wasm")
      list(LENGTH existing_wasm_files existing_wasm_count)
      if(existing_wasm_count EQUAL 1)
        list(GET existing_wasm_files 0 wasm_file)
      endif()
    endif()
  endif()

  if(NOT EXISTS "${wasm_file}")
    skip_example("${example}" "expected Wasm artifact is missing: ${wasm_file}")
    continue()
  endif()

  set(asm_file "${OUTPUT_DIR}/artifacts/${example}.asm")
  set(vbin_file "${OUTPUT_DIR}/artifacts/${example}.vbin")
  run_checked("${example} wasm2vircon translation"
    "${WASM2VIRCON_EXECUTABLE}" "${wasm_file}" --entry "${entry}" -o "${asm_file}")
  run_checked("${example} assembly"
    "${ASSEMBLE_EXECUTABLE}" -o "${vbin_file}" "${asm_file}")
  set(runtime_metric "not_available")
  if(PACKROM_EXECUTABLE)
    set(rom_xml "${OUTPUT_DIR}/artifacts/${example}.rom.xml")
    set(rom_file "${OUTPUT_DIR}/artifacts/${example}.v32")
    file(WRITE "${rom_xml}" "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rom-definition version=\"1.0\">\n  <rom type=\"cartridge\" title=\"Benchmark Baseline\" version=\"1.0\"/>\n  <binary path=\"${example}.vbin\"/>\n  <textures/>\n  <sounds/>\n</rom-definition>\n")
    run_checked("${example} ROM packaging" "${PACKROM_EXECUTABLE}" -o "${rom_file}" "${rom_xml}")
    profile_benchmarks("${example}" "${rom_file}" runtime_metric)
  endif()
  collect_metrics("${example}" "${language}" "${wasm_file}" "${asm_file}" "${vbin_file}" "${runtime_metric}")
  math(EXPR collected_count "${collected_count} + 1")
endforeach()

if(collected_count EQUAL 0)
  message(FATAL_ERROR "no optimization-baseline examples were collected")
endif()

get_property(case_rows GLOBAL PROPERTY BENCHMARK_CASE_ROWS)
if(case_rows)
  file(APPEND "${REPORT_MD}" "\n## Tour cases\n\n| Example | Case | Count | FPS | GPU % | CPU % |\n| --- | --- | ---: | ---: | ---: | ---: |\n")
  foreach(case_row IN LISTS case_rows)
    string(REPLACE "|" " | " case_row "${case_row}")
    file(APPEND "${REPORT_MD}" "| ${case_row} |\n")
  endforeach()
endif()

file(APPEND "${REPORT_MD}"
  "\n## Runtime measurements\n\n"
  "The benchmark ROMs run a Start-triggered tour across multiple benchmark tests. "
  "Each runs for at least 60 display frames before interactive mode resumes. "
  "The aggregate instruction count includes BIOS startup, initialization, and every scenario; "
  "it is not a per-frame timing or a GPU measurement. The per-address v32sim profile and simulator output "
  "are in `artifacts/` when a compatible simulator and BIOS are available.\n\n"
  "See `frames.csv` for every generated function/helper frame and `environment.txt` for the measured compiler and simulator identities.\n")

print_benchmark_summary()
