# Optional CTest entry for the standalone example benchmark collector.
cmake_minimum_required(VERSION 3.20)
project(vircon_example_benchmarks NONE)
include(CTest)

# Keep the benchmark opt-in and independent from the compiler's test suite.
set(BENCHMARK_EXAMPLES "c-boids;cpp-boids;zig-boids;rust-boids;tinygo-boids" CACHE STRING
  "Semicolon-separated example names")
option(BENCHMARK_ENABLE_V32SIM "Run benchmark tours when v32sim is available" ON)
option(BENCHMARK_REQUIRE_ALL "Fail if any selected example cannot be built" OFF)
set(WASM2VIRCON_EXECUTABLE "" CACHE FILEPATH "Optional wasm2vircon executable")
set(V32SIM_EXECUTABLE "" CACHE FILEPATH "Optional v32sim executable")
set(V32SIM_BIOS "" CACHE FILEPATH "Optional Vircon32 BIOS ROM")

if(BUILD_TESTING)
  string(REPLACE ";" "\\;" benchmark_examples_arg "${BENCHMARK_EXAMPLES}")
  set(benchmark_args
    "-DEXAMPLES=${benchmark_examples_arg}"
    "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/results"
    "-DENABLE_V32SIM=${BENCHMARK_ENABLE_V32SIM}"
    "-DREQUIRE_ALL=${BENCHMARK_REQUIRE_ALL}")
  foreach(option_name IN ITEMS WASM2VIRCON_EXECUTABLE V32SIM_EXECUTABLE V32SIM_BIOS)
    if(NOT "${${option_name}}" STREQUAL "")
      list(APPEND benchmark_args "-D${option_name}=${${option_name}}")
    endif()
  endforeach()
  add_test(NAME example-benchmark
    COMMAND "${CMAKE_COMMAND}" ${benchmark_args} -P "${CMAKE_CURRENT_SOURCE_DIR}/benchmark.cmake")
  set_tests_properties(example-benchmark PROPERTIES LABELS "benchmark" TIMEOUT 300)
endif()
