cmake_minimum_required(VERSION 3.20)

project(wasm2vircon VERSION 0.1.0 LANGUAGES C)

# Respect Binaryen_ROOT with CMake versions that know the modern package-root
# policy, while remaining compatible with the project's CMake 3.20 minimum.
if(POLICY CMP0144)
  cmake_policy(SET CMP0144 NEW)
endif()

# Keep warning policy consistent without passing GNU-only flags to MSVC.
function(wasm2vircon_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endfunction()

# Prefer Binaryen's exported CMake package.  A package target carries its own
# include directories and, importantly, any C++ runtime/system dependencies
# required by a static Binaryen archive.  Some distro packages still install
# only a header and library, so retain a conventional fallback below.
find_package(Binaryen CONFIG QUIET)

set(WASM2VIRCON_BINARYEN_TARGET "")
foreach(candidate IN ITEMS Binaryen::Binaryen Binaryen::binaryen binaryen::binaryen binaryen)
  if(TARGET "${candidate}")
    set(WASM2VIRCON_BINARYEN_TARGET "${candidate}")
    break()
  endif()
endforeach()

set(WASM2VIRCON_BINARYEN_IS_STATIC OFF)
set(WASM2VIRCON_BINARYEN_NEEDS_MANUAL_STATIC_DEPS OFF)

if(WASM2VIRCON_BINARYEN_TARGET)
  # An imported target is the preferred package interface.  Determine whether
  # it is static so link-mode selection can use it accurately.
  get_target_property(WASM2VIRCON_BINARYEN_TARGET_TYPE
    "${WASM2VIRCON_BINARYEN_TARGET}" TYPE)
  if(WASM2VIRCON_BINARYEN_TARGET_TYPE STREQUAL "STATIC_LIBRARY")
    set(WASM2VIRCON_BINARYEN_IS_STATIC ON)
  endif()
  set(WASM2VIRCON_BINARYEN_INCLUDE_DIR "")
  set(WASM2VIRCON_BINARYEN_LIBRARY "${WASM2VIRCON_BINARYEN_TARGET}")
  message(STATUS "Using Binaryen CMake package target: ${WASM2VIRCON_BINARYEN_TARGET}")
else()
  # Legacy Binaryen installations do not provide a package config.  Locate
  # their C API explicitly and add static-library dependencies ourselves.
  find_path(WASM2VIRCON_BINARYEN_INCLUDE_DIR binaryen-c.h)
  find_library(WASM2VIRCON_BINARYEN_LIBRARY NAMES binaryen)
  if(NOT WASM2VIRCON_BINARYEN_INCLUDE_DIR OR NOT WASM2VIRCON_BINARYEN_LIBRARY)
    message(FATAL_ERROR
      "Binaryen C API was not found. Install Binaryen's CMake package or development files "
      "(header binaryen-c.h and library binaryen), then configure again.")
  endif()
  get_filename_component(WASM2VIRCON_BINARYEN_EXTENSION
    "${WASM2VIRCON_BINARYEN_LIBRARY}" EXT)
  if(WASM2VIRCON_BINARYEN_EXTENSION STREQUAL "${CMAKE_STATIC_LIBRARY_SUFFIX}")
    set(WASM2VIRCON_BINARYEN_IS_STATIC ON)
    set(WASM2VIRCON_BINARYEN_NEEDS_MANUAL_STATIC_DEPS ON)
  endif()
  message(STATUS "Using Binaryen header/library fallback")
endif()

# Binaryen normalization is always enabled because Binaryen is already the
# required Wasm decoder. This option controls linkage only: AUTO prefers a
# static archive, ON requires one, and OFF uses the normally discovered library.
set(WASM2VIRCON_EMBEDDED_BINARYEN "AUTO" CACHE STRING
  "Prefer static Binaryen linkage (AUTO, ON, or OFF)")
set_property(CACHE WASM2VIRCON_EMBEDDED_BINARYEN PROPERTY STRINGS AUTO ON OFF)
string(TOUPPER "${WASM2VIRCON_EMBEDDED_BINARYEN}" WASM2VIRCON_EMBEDDED_BINARYEN)
if(NOT WASM2VIRCON_EMBEDDED_BINARYEN STREQUAL "AUTO"
   AND NOT WASM2VIRCON_EMBEDDED_BINARYEN STREQUAL "ON"
   AND NOT WASM2VIRCON_EMBEDDED_BINARYEN STREQUAL "OFF")
  message(FATAL_ERROR
    "WASM2VIRCON_EMBEDDED_BINARYEN must be AUTO, ON, or OFF")
endif()

# The package's imported target is reusable for static linkage when it is
# static. For legacy discovery, look for the host platform's conventional
# static-library name beside the selected library; do not assume libbinaryen.a.
if(WASM2VIRCON_BINARYEN_TARGET AND WASM2VIRCON_BINARYEN_IS_STATIC)
  set(WASM2VIRCON_BINARYEN_STATIC_LIBRARY "${WASM2VIRCON_BINARYEN_TARGET}")
elseif(NOT WASM2VIRCON_BINARYEN_TARGET)
  unset(WASM2VIRCON_BINARYEN_STATIC_LIBRARY CACHE)
  unset(WASM2VIRCON_BINARYEN_STATIC_LIBRARY)
  get_filename_component(WASM2VIRCON_BINARYEN_LIBRARY_DIRECTORY
    "${WASM2VIRCON_BINARYEN_LIBRARY}" DIRECTORY)
  set(WASM2VIRCON_BINARYEN_STATIC_LIBRARY_NAMES
    "${CMAKE_STATIC_LIBRARY_PREFIX}binaryen${CMAKE_STATIC_LIBRARY_SUFFIX}"
    "binaryen${CMAKE_STATIC_LIBRARY_SUFFIX}")
  list(REMOVE_DUPLICATES WASM2VIRCON_BINARYEN_STATIC_LIBRARY_NAMES)
  find_file(WASM2VIRCON_BINARYEN_STATIC_LIBRARY
    NAMES ${WASM2VIRCON_BINARYEN_STATIC_LIBRARY_NAMES}
    HINTS "${WASM2VIRCON_BINARYEN_LIBRARY_DIRECTORY}")
else()
  set(WASM2VIRCON_BINARYEN_STATIC_LIBRARY "")
endif()

set(WASM2VIRCON_BINARYEN_LINK_LIBRARY "${WASM2VIRCON_BINARYEN_LIBRARY}")
if(WASM2VIRCON_EMBEDDED_BINARYEN STREQUAL "ON")
  if(NOT WASM2VIRCON_BINARYEN_STATIC_LIBRARY)
    message(FATAL_ERROR
      "WASM2VIRCON_EMBEDDED_BINARYEN=ON requires Binaryen's static library. Install the "
      "static Binaryen development archive, or select AUTO/OFF for the normal library selection.")
  endif()
  set(WASM2VIRCON_BINARYEN_LINK_LIBRARY "${WASM2VIRCON_BINARYEN_STATIC_LIBRARY}")
  set(WASM2VIRCON_BINARYEN_IS_STATIC ON)
  if(NOT WASM2VIRCON_BINARYEN_TARGET)
    set(WASM2VIRCON_BINARYEN_NEEDS_MANUAL_STATIC_DEPS ON)
  endif()
elseif(WASM2VIRCON_EMBEDDED_BINARYEN STREQUAL "AUTO")
  if(WASM2VIRCON_BINARYEN_STATIC_LIBRARY)
    set(WASM2VIRCON_BINARYEN_LINK_LIBRARY "${WASM2VIRCON_BINARYEN_STATIC_LIBRARY}")
    set(WASM2VIRCON_BINARYEN_IS_STATIC ON)
    if(NOT WASM2VIRCON_BINARYEN_TARGET)
      set(WASM2VIRCON_BINARYEN_NEEDS_MANUAL_STATIC_DEPS ON)
    endif()
  endif()
elseif(WASM2VIRCON_BINARYEN_IS_STATIC)
  message(FATAL_ERROR
    "WASM2VIRCON_EMBEDDED_BINARYEN=OFF requires a dynamically linked Binaryen library, "
    "but discovery selected a static archive. Install Binaryen's shared library or use AUTO/ON.")
endif()

add_executable(wasm2vircon
  src/main.c
  src/diagnostics.c
  src/wasm_module.c
  src/wasm_dwarf.c
  src/legalize.c
  src/validator.c
  src/vircon_ir.c
  src/lowering.c
  src/emitter.c
)

target_compile_features(wasm2vircon PRIVATE c_std_11)
wasm2vircon_enable_warnings(wasm2vircon)
target_include_directories(wasm2vircon PRIVATE src)
if(WASM2VIRCON_BINARYEN_INCLUDE_DIR)
  target_include_directories(wasm2vircon PRIVATE "${WASM2VIRCON_BINARYEN_INCLUDE_DIR}")
endif()

# A legacy static archive needs explicit C++ runtime/system libraries. An
# exported Binaryen target owns those transitive dependencies itself.
target_link_libraries(wasm2vircon PRIVATE ${WASM2VIRCON_BINARYEN_LINK_LIBRARY})
if(WASM2VIRCON_BINARYEN_IS_STATIC AND WASM2VIRCON_BINARYEN_NEEDS_MANUAL_STATIC_DEPS)
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
if(WASM2VIRCON_BINARYEN_IS_STATIC)
  message(STATUS "Using static Binaryen linkage with in-process normalization")
else()
  message(STATUS "Using dynamic Binaryen linkage with in-process normalization")
endif()

# Normal builds should produce only the compiler. CTest's module defaults this
# option to ON, so define the standard switch first and keep the full test
# suite an explicit developer/CI action.
option(BUILD_TESTING "Build and register wasm2vircon tests" OFF)
include(CTest)
if(BUILD_TESTING)
  # Small native unit tests live in self-contained directories just like the
  # source-to-ROM fixtures below.
  add_executable(wasm2vircon-memory-model-test tests/unit-memory-model/test.c)
  target_compile_features(wasm2vircon-memory-model-test PRIVATE c_std_11)
  wasm2vircon_enable_warnings(wasm2vircon-memory-model-test)
  add_test(NAME wasm2vircon-memory-model COMMAND wasm2vircon-memory-model-test)

  add_executable(wasm2vircon-u32-to-f32-test tests/unit-u32-to-f32/test.c)
  target_compile_features(wasm2vircon-u32-to-f32-test PRIVATE c_std_11)
  wasm2vircon_enable_warnings(wasm2vircon-u32-to-f32-test)
  add_test(NAME wasm2vircon-u32-to-f32 COMMAND wasm2vircon-u32-to-f32-test)

  add_executable(wasm2vircon-vircon-ir-test
    tests/unit-vircon-ir/test.c
    src/vircon_ir.c
    src/diagnostics.c
  )
  target_compile_features(wasm2vircon-vircon-ir-test PRIVATE c_std_11)
  target_include_directories(wasm2vircon-vircon-ir-test PRIVATE src)
  wasm2vircon_enable_warnings(wasm2vircon-vircon-ir-test)
  add_test(NAME wasm2vircon-vircon-ir COMMAND wasm2vircon-vircon-ir-test)

  add_executable(wasm2vircon-dwarf-fixture tests/dwarf/fixture.c)
  target_compile_features(wasm2vircon-dwarf-fixture PRIVATE c_std_11)
  wasm2vircon_enable_warnings(wasm2vircon-dwarf-fixture)

  find_program(WASM_AS_EXECUTABLE wasm-as REQUIRED)
  find_program(VIRCON_ASSEMBLE_EXECUTABLE assemble REQUIRED)

  add_test(
    NAME wasm2vircon-dwarf-input
    COMMAND ${CMAKE_COMMAND}
      -DTOOL=$<TARGET_FILE:wasm2vircon>
      -DWASM_AS=${WASM_AS_EXECUTABLE}
      -DASSEMBLE=${VIRCON_ASSEMBLE_EXECUTABLE}
      -DFIXTURE_GENERATOR=$<TARGET_FILE:wasm2vircon-dwarf-fixture>
      -DTESTS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/dwarf-input-test.cmake
  )

  add_test(
    NAME wasm2vircon-cli-inspection
    COMMAND ${CMAKE_COMMAND}
      -DTOOL=$<TARGET_FILE:wasm2vircon>
      -DWASM_AS=${WASM_AS_EXECUTABLE}
      -DTESTS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli-inspection-test.cmake
  )

  # Read one metadata field while configuring individual CTest cases. The
  # execution runners remain responsible for full metadata validation.
  function(wasm2vircon_test_metadata metadata_file key output_variable)
    file(STRINGS "${metadata_file}" metadata_value REGEX "^${key}=" LIMIT_COUNT 1)
    if(metadata_value)
      string(REGEX REPLACE "^[^=]*=" "" metadata_value "${metadata_value}")
    endif()
    set(${output_variable} "${metadata_value}" PARENT_SCOPE)
  endfunction()

  # Expose every WAT fixture as its own CTest entry instead of hiding all
  # profile checks behind one umbrella test.
  file(GLOB wasm_case_metadata CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/wat-*/testcases")
  foreach(metadata_file IN LISTS wasm_case_metadata)
    get_filename_component(case_directory "${metadata_file}" DIRECTORY)
    get_filename_component(case_name "${case_directory}" NAME)
    wasm2vircon_test_metadata("${metadata_file}" source case_source)
    wasm2vircon_test_metadata("${metadata_file}" cli_only case_cli_only)
    if(case_source MATCHES "\\.wat$" AND NOT case_cli_only STREQUAL "true")
      add_test(
        NAME ${case_name}
        COMMAND ${CMAKE_COMMAND}
          -DTOOL=$<TARGET_FILE:wasm2vircon>
          -DWASM_AS=${WASM_AS_EXECUTABLE}
          -DASSEMBLE=${VIRCON_ASSEMBLE_EXECUTABLE}
          -DTESTS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests
          -DCASE_DIRECTORY=${case_directory}
          -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-wasm-tests.cmake
      )
      set_tests_properties(${case_name} PROPERTIES LABELS wasm)
    endif()
  endforeach()

  find_program(CLANG_EXECUTABLE clang REQUIRED)
  set(WASM2VIRCON_CXX_COMPILER "" CACHE FILEPATH
    "WASI-capable clang++ used for optional official-libc++ integration tests")
  set(WASM2VIRCON_CXX_SYSROOT "" CACHE PATH
    "Optional WASI sysroot passed to the C++ integration compiler")
  find_program(WASM_LD_EXECUTABLE wasm-ld REQUIRED)
  find_program(VIRCON_PACKROM_EXECUTABLE packrom REQUIRED)
  find_program(VIRCON_PNG2VIRCON_EXECUTABLE png2vircon REQUIRED)
  find_program(VIRCON_WAV2VIRCON_EXECUTABLE wav2vircon REQUIRED)
  find_program(VIRCON_TILED2VIRCON_EXECUTABLE tiled2vircon REQUIRED)

  # Likewise, compile/package each C or freestanding-C++ fixture independently
  # so CTest reports the exact application or API case that fails.
  file(GLOB rom_case_metadata CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*/testcases")
  foreach(metadata_file IN LISTS rom_case_metadata)
    get_filename_component(case_directory "${metadata_file}" DIRECTORY)
    get_filename_component(case_name "${case_directory}" NAME)
    wasm2vircon_test_metadata("${metadata_file}" source case_source)
    if(case_source MATCHES "\\.(cc|cpp|cxx)$" AND NOT WASM2VIRCON_CXX_COMPILER)
      message(STATUS
        "Skipping ${case_name}; set WASM2VIRCON_CXX_COMPILER to a WASI-capable clang++ to test libc++")
    elseif(case_source MATCHES "\\.(c|cc|cpp|cxx)$")
      add_test(
        NAME ${case_name}
        COMMAND ${CMAKE_COMMAND}
          -DTOOL=$<TARGET_FILE:wasm2vircon>
          -DCLANG=${CLANG_EXECUTABLE}
          -DCXX=${WASM2VIRCON_CXX_COMPILER}
          -DCXX_SYSROOT=${WASM2VIRCON_CXX_SYSROOT}
          -DWASM_LD=${WASM_LD_EXECUTABLE}
          -DASSEMBLE=${VIRCON_ASSEMBLE_EXECUTABLE}
          -DPACKROM=${VIRCON_PACKROM_EXECUTABLE}
          -DPNG2VIRCON=${VIRCON_PNG2VIRCON_EXECUTABLE}
          -DWAV2VIRCON=${VIRCON_WAV2VIRCON_EXECUTABLE}
          -DTILED2VIRCON=${VIRCON_TILED2VIRCON_EXECUTABLE}
          -DTESTS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests
          -DCASE_DIRECTORY=${case_directory}
          -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-rom-tests.cmake
      )
      set_tests_properties(${case_name} PROPERTIES LABELS "integration;rom")
    endif()
  endforeach()

  set(WASM2VIRCON_V32SIM_EXECUTABLE "" CACHE FILEPATH
    "Path to an externally built v32sim executable for optional execution tests")
  set(WASM2VIRCON_V32SIM_BIOS "" CACHE FILEPATH
    "Path to an installed Vircon32 BIOS .v32 for optional execution tests")
  if(WASM2VIRCON_V32SIM_EXECUTABLE AND WASM2VIRCON_V32SIM_BIOS)
    foreach(metadata_file IN LISTS rom_case_metadata)
      get_filename_component(case_directory "${metadata_file}" DIRECTORY)
      get_filename_component(case_name "${case_directory}" NAME)
      wasm2vircon_test_metadata("${metadata_file}" sim_commands case_sim_commands)
      if(case_sim_commands)
        set(simulator_test_name "${case_name}-simulator")
        add_test(
          NAME ${simulator_test_name}
          COMMAND ${CMAKE_COMMAND}
            -DTOOL=$<TARGET_FILE:wasm2vircon>
            -DCLANG=${CLANG_EXECUTABLE}
            -DCXX=${WASM2VIRCON_CXX_COMPILER}
            -DCXX_SYSROOT=${WASM2VIRCON_CXX_SYSROOT}
            -DWASM_LD=${WASM_LD_EXECUTABLE}
            -DASSEMBLE=${VIRCON_ASSEMBLE_EXECUTABLE}
            -DPACKROM=${VIRCON_PACKROM_EXECUTABLE}
            -DPNG2VIRCON=${VIRCON_PNG2VIRCON_EXECUTABLE}
            -DWAV2VIRCON=${VIRCON_WAV2VIRCON_EXECUTABLE}
            -DTILED2VIRCON=${VIRCON_TILED2VIRCON_EXECUTABLE}
            -DTESTS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests
            -DCASE_DIRECTORY=${case_directory}
            -DTEST_MODE=simulator
            -DV32SIM=${WASM2VIRCON_V32SIM_EXECUTABLE}
            -DV32SIM_BIOS=${WASM2VIRCON_V32SIM_BIOS}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-rom-tests.cmake
        )
        set_tests_properties(${simulator_test_name} PROPERTIES LABELS simulator)
      endif()
    endforeach()
  else()
    message(STATUS
      "v32sim execution tests are disabled; set WASM2VIRCON_V32SIM_EXECUTABLE "
      "and WASM2VIRCON_V32SIM_BIOS to enable them")
  endif()
endif()
