cmake_minimum_required(VERSION 3.20)

project(wasm2vircon VERSION 0.1.0 LANGUAGES C)

find_path(BINARYEN_INCLUDE_DIR binaryen-c.h)
find_library(BINARYEN_LIBRARY NAMES binaryen)

if(NOT BINARYEN_INCLUDE_DIR OR NOT BINARYEN_LIBRARY)
  message(FATAL_ERROR
    "Binaryen C API was not found. Install Binaryen development files "
    "(header binaryen-c.h and library libbinaryen), then configure again.")
endif()

add_executable(wasm2vircon
  src/main.c
  src/diagnostics.c
  src/wasm_module.c
  src/validator.c
  src/vircon_ir.c
  src/lowering.c
  src/emitter.c
)

target_compile_features(wasm2vircon PRIVATE c_std_11)
target_compile_options(wasm2vircon PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_include_directories(wasm2vircon PRIVATE src ${BINARYEN_INCLUDE_DIR})
target_link_libraries(wasm2vircon PRIVATE ${BINARYEN_LIBRARY})

include(CTest)
if(BUILD_TESTING)
  add_executable(wasm2vircon-memory-model-test tests/memory-model-test.c)
  target_compile_features(wasm2vircon-memory-model-test PRIVATE c_std_11)
  target_compile_options(wasm2vircon-memory-model-test PRIVATE -Wall -Wextra -Wpedantic -Werror)
  add_test(NAME wasm2vircon-memory-model COMMAND wasm2vircon-memory-model-test)

  find_program(WASM_AS_EXECUTABLE wasm-as REQUIRED)
  find_program(CLANG_EXECUTABLE clang REQUIRED)
  find_program(VIRCON_ASSEMBLE_EXECUTABLE assemble REQUIRED)
  find_program(VIRCON_PACKROM_EXECUTABLE packrom REQUIRED)
  find_program(VIRCON_PNG2VIRCON_EXECUTABLE png2vircon REQUIRED)
  find_program(VIRCON_WAV2VIRCON_EXECUTABLE wav2vircon REQUIRED)
  find_program(VIRCON_TILED2VIRCON_EXECUTABLE tiled2vircon REQUIRED)

  add_test(
    NAME wasm2vircon-profile
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-tests.sh
      $<TARGET_FILE:wasm2vircon>
      ${CLANG_EXECUTABLE}
      ${WASM_AS_EXECUTABLE}
      ${VIRCON_ASSEMBLE_EXECUTABLE}
      ${VIRCON_PACKROM_EXECUTABLE}
      ${VIRCON_PNG2VIRCON_EXECUTABLE}
      ${VIRCON_WAV2VIRCON_EXECUTABLE}
      ${VIRCON_TILED2VIRCON_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tests
  )
endif()
