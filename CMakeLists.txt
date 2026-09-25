cmake_minimum_required(VERSION 3.20)

project(wasm2vircon VERSION 0.1.0 LANGUAGES C)

# Keep warning policy consistent without passing GNU-only flags to MSVC.
function(wasm2vircon_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endfunction()

find_path(BINARYEN_INCLUDE_DIR binaryen-c.h)
find_library(BINARYEN_LIBRARY NAMES binaryen)

if(NOT BINARYEN_INCLUDE_DIR OR NOT BINARYEN_LIBRARY)
  message(FATAL_ERROR
    "Binaryen C API was not found. Install Binaryen development files "
    "(header binaryen-c.h and library libbinaryen), then configure again.")
endif()

# The decoder always uses Binaryen's C API. This option controls only the
# additional in-memory cleanup pass before the compiler-owned frontend model is
# built. AUTO enables it when a static Binaryen archive is installed; OFF keeps
# the established external wasm-opt normalization contract.
set(WASM2VIRCON_EMBEDDED_BINARYEN "AUTO" CACHE STRING
  "Use static Binaryen for in-memory Wasm normalization (AUTO, ON, or OFF)")
set_property(CACHE WASM2VIRCON_EMBEDDED_BINARYEN PROPERTY STRINGS AUTO ON OFF)
string(TOUPPER "${WASM2VIRCON_EMBEDDED_BINARYEN}" WASM2VIRCON_EMBEDDED_BINARYEN)
if(NOT WASM2VIRCON_EMBEDDED_BINARYEN STREQUAL "AUTO"
   AND NOT WASM2VIRCON_EMBEDDED_BINARYEN STREQUAL "ON"
   AND NOT WASM2VIRCON_EMBEDDED_BINARYEN STREQUAL "OFF")
  message(FATAL_ERROR
    "WASM2VIRCON_EMBEDDED_BINARYEN must be AUTO, ON, or OFF")
endif()

# Look for the host platform's conventional static-library name.  Do not
# assume Unix's libbinaryen.a: Windows commonly uses binaryen.lib.
get_filename_component(BINARYEN_LIBRARY_DIRECTORY "${BINARYEN_LIBRARY}" DIRECTORY)
set(BINARYEN_STATIC_LIBRARY_NAMES
  "${CMAKE_STATIC_LIBRARY_PREFIX}binaryen${CMAKE_STATIC_LIBRARY_SUFFIX}"
  "binaryen${CMAKE_STATIC_LIBRARY_SUFFIX}")
list(REMOVE_DUPLICATES BINARYEN_STATIC_LIBRARY_NAMES)
find_file(BINARYEN_STATIC_LIBRARY
  NAMES ${BINARYEN_STATIC_LIBRARY_NAMES}
  HINTS "${BINARYEN_LIBRARY_DIRECTORY}")
set(WASM2VIRCON_USE_EMBEDDED_BINARYEN OFF)
if(WASM2VIRCON_EMBEDDED_BINARYEN STREQUAL "ON")
  if(NOT BINARYEN_STATIC_LIBRARY)
    message(FATAL_ERROR
      "WASM2VIRCON_EMBEDDED_BINARYEN=ON requires Binaryen's static library. Install the "
      "static Binaryen development archive, or select OFF to use wasm-opt.")
  endif()
  set(WASM2VIRCON_USE_EMBEDDED_BINARYEN ON)
elseif(WASM2VIRCON_EMBEDDED_BINARYEN STREQUAL "AUTO")
  if(BINARYEN_STATIC_LIBRARY)
    set(WASM2VIRCON_USE_EMBEDDED_BINARYEN ON)
  else()
    message(STATUS
      "Static Binaryen library is unavailable; retaining external wasm-opt normalization")
  endif()
endif()

add_executable(wasm2vircon
  src/main.c
  src/diagnostics.c
  src/wasm_module.c
  src/legalize.c
  src/validator.c
  src/vircon_ir.c
  src/lowering.c
  src/emitter.c
)

target_compile_features(wasm2vircon PRIVATE c_std_11)
wasm2vircon_enable_warnings(wasm2vircon)
target_include_directories(wasm2vircon PRIVATE src ${BINARYEN_INCLUDE_DIR})

# Even an external-normalization build needs Binaryen for Wasm decoding. If
# CMake selects a static archive for that dependency, it needs the same C++ and
# system libraries as an explicitly embedded archive.
set(WASM2VIRCON_BINARYEN_LINK_LIBRARY ${BINARYEN_LIBRARY})
set(WASM2VIRCON_BINARYEN_IS_STATIC OFF)
if(WASM2VIRCON_USE_EMBEDDED_BINARYEN)
  set(WASM2VIRCON_BINARYEN_LINK_LIBRARY ${BINARYEN_STATIC_LIBRARY})
  set(WASM2VIRCON_BINARYEN_IS_STATIC ON)
else()
  get_filename_component(BINARYEN_LIBRARY_EXTENSION "${BINARYEN_LIBRARY}" EXT)
  if(BINARYEN_LIBRARY_EXTENSION STREQUAL CMAKE_STATIC_LIBRARY_SUFFIX)
    set(WASM2VIRCON_BINARYEN_IS_STATIC ON)
  endif()
endif()

target_link_libraries(wasm2vircon PRIVATE ${WASM2VIRCON_BINARYEN_LINK_LIBRARY})
if(WASM2VIRCON_BINARYEN_IS_STATIC)
  # Binaryen is implemented in C++, although wasm2vircon itself remains C.
  # Link through the host C++ driver on Windows, where C++ archive dependencies
  # are normally supplied by the toolchain. Unix-like toolchains need their
  # runtime and system dependencies listed explicitly.
  if(MSVC)
    enable_language(CXX)
    set_property(TARGET wasm2vircon PROPERTY LINKER_LANGUAGE CXX)
  else()
    find_library(BINARYEN_CXX_RUNTIME NAMES stdc++ c++)
    if(NOT BINARYEN_CXX_RUNTIME)
      message(FATAL_ERROR
        "Static Binaryen requires a C++ standard library (libstdc++ or libc++)")
    endif()
    find_package(Threads REQUIRED)
    find_library(BINARYEN_MATH_LIBRARY NAMES m)
    target_link_libraries(wasm2vircon PRIVATE ${BINARYEN_CXX_RUNTIME}
      Threads::Threads ${CMAKE_DL_LIBS} ${BINARYEN_MATH_LIBRARY})
  endif()
endif()
if(WASM2VIRCON_USE_EMBEDDED_BINARYEN)
  target_compile_definitions(wasm2vircon PRIVATE USE_EMBEDDED_BINARYEN=1)
  message(STATUS "Using embedded static Binaryen normalization")
else()
  message(STATUS "Using external wasm-opt normalization")
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(wasm2vircon-memory-model-test tests/memory-model-test.c)
  target_compile_features(wasm2vircon-memory-model-test PRIVATE c_std_11)
  wasm2vircon_enable_warnings(wasm2vircon-memory-model-test)
  add_test(NAME wasm2vircon-memory-model COMMAND wasm2vircon-memory-model-test)

  add_executable(wasm2vircon-u32-to-f32-test tests/u32-to-f32-test.c)
  target_compile_features(wasm2vircon-u32-to-f32-test PRIVATE c_std_11)
  wasm2vircon_enable_warnings(wasm2vircon-u32-to-f32-test)
  add_test(NAME wasm2vircon-u32-to-f32 COMMAND wasm2vircon-u32-to-f32-test)

  find_program(WASM_AS_EXECUTABLE wasm-as REQUIRED)
  find_program(VIRCON_ASSEMBLE_EXECUTABLE assemble REQUIRED)

  add_test(
    NAME wasm2vircon-cli-inspection
    COMMAND ${CMAKE_COMMAND}
      -DTOOL=$<TARGET_FILE:wasm2vircon>
      -DWASM_AS=${WASM_AS_EXECUTABLE}
      -DTESTS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli-inspection-test.cmake
  )

  add_test(
    NAME wasm2vircon-wasm-profile
    COMMAND ${CMAKE_COMMAND}
      -DTOOL=$<TARGET_FILE:wasm2vircon>
      -DWASM_AS=${WASM_AS_EXECUTABLE}
      -DASSEMBLE=${VIRCON_ASSEMBLE_EXECUTABLE}
      -DTESTS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-wasm-tests.cmake
  )

  find_program(CLANG_EXECUTABLE clang REQUIRED)
  find_program(WASM_LD_EXECUTABLE wasm-ld REQUIRED)
  find_program(VIRCON_PACKROM_EXECUTABLE packrom REQUIRED)
  find_program(VIRCON_PNG2VIRCON_EXECUTABLE png2vircon REQUIRED)
  find_program(VIRCON_WAV2VIRCON_EXECUTABLE wav2vircon REQUIRED)
  find_program(VIRCON_TILED2VIRCON_EXECUTABLE tiled2vircon REQUIRED)

  add_test(
    NAME wasm2vircon-rom-profile
    COMMAND ${CMAKE_COMMAND}
      -DTOOL=$<TARGET_FILE:wasm2vircon>
      -DCLANG=${CLANG_EXECUTABLE}
      -DWASM_LD=${WASM_LD_EXECUTABLE}
      -DASSEMBLE=${VIRCON_ASSEMBLE_EXECUTABLE}
      -DPACKROM=${VIRCON_PACKROM_EXECUTABLE}
      -DPNG2VIRCON=${VIRCON_PNG2VIRCON_EXECUTABLE}
      -DWAV2VIRCON=${VIRCON_WAV2VIRCON_EXECUTABLE}
      -DTILED2VIRCON=${VIRCON_TILED2VIRCON_EXECUTABLE}
      -DTESTS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-rom-tests.cmake
  )
  set_tests_properties(wasm2vircon-rom-profile PROPERTIES LABELS integration)

  set(WASM2VIRCON_V32SIM_EXECUTABLE "" CACHE FILEPATH
    "Path to an externally built v32sim executable for optional execution tests")
  set(WASM2VIRCON_V32SIM_BIOS "" CACHE FILEPATH
    "Path to an installed Vircon32 BIOS .v32 for optional execution tests")
  if(WASM2VIRCON_V32SIM_EXECUTABLE AND WASM2VIRCON_V32SIM_BIOS)
    add_test(
      NAME wasm2vircon-simulator
      COMMAND ${CMAKE_COMMAND}
        -DTOOL=$<TARGET_FILE:wasm2vircon>
        -DCLANG=${CLANG_EXECUTABLE}
        -DWASM_LD=${WASM_LD_EXECUTABLE}
        -DASSEMBLE=${VIRCON_ASSEMBLE_EXECUTABLE}
        -DPACKROM=${VIRCON_PACKROM_EXECUTABLE}
        -DPNG2VIRCON=${VIRCON_PNG2VIRCON_EXECUTABLE}
        -DWAV2VIRCON=${VIRCON_WAV2VIRCON_EXECUTABLE}
        -DTILED2VIRCON=${VIRCON_TILED2VIRCON_EXECUTABLE}
        -DTESTS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests
        -DTEST_MODE=simulator
        -DV32SIM=${WASM2VIRCON_V32SIM_EXECUTABLE}
        -DV32SIM_BIOS=${WASM2VIRCON_V32SIM_BIOS}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-rom-tests.cmake
    )
    set_tests_properties(wasm2vircon-simulator PROPERTIES LABELS simulator)
  else()
    message(STATUS
      "v32sim execution tests are disabled; set WASM2VIRCON_V32SIM_EXECUTABLE and WASM2VIRCON_V32SIM_BIOS to enable them")
  endif()
endif()
