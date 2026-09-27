cmake_minimum_required(VERSION 3.20)

# Change this one value when copying the sample into a new cartridge project.
set(project_name "CBoids")
project(${project_name} NONE)

get_filename_component(WASM2VIRCON_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# An explicit cache value wins. Otherwise, prefer the normal PATH installation
# and fall back to a compiler built in this checkout for contributor convenience.
set(WASM2VIRCON_EXECUTABLE "" CACHE FILEPATH "Path to the wasm2vircon executable")
if(NOT WASM2VIRCON_EXECUTABLE)
  unset(WASM2VIRCON_EXECUTABLE CACHE)
  find_program(WASM2VIRCON_EXECUTABLE NAMES wasm2vircon)
endif()
if(NOT WASM2VIRCON_EXECUTABLE)
  unset(WASM2VIRCON_EXECUTABLE CACHE)
  find_program(WASM2VIRCON_EXECUTABLE NAMES wasm2vircon
    HINTS "${WASM2VIRCON_ROOT}" "${WASM2VIRCON_ROOT}/build" NO_DEFAULT_PATH)
endif()
if(NOT WASM2VIRCON_EXECUTABLE)
  message(FATAL_ERROR "wasm2vircon was not found on PATH; set WASM2VIRCON_EXECUTABLE explicitly")
endif()

find_program(CLANG_EXECUTABLE NAMES clang REQUIRED)
find_program(WASM_LD_EXECUTABLE NAMES wasm-ld REQUIRED)
find_program(ASSEMBLE_EXECUTABLE NAMES assemble REQUIRED)
find_program(PACKROM_EXECUTABLE NAMES packrom REQUIRED)

set(project_main_source "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")
set(project_boids_source "${CMAKE_CURRENT_SOURCE_DIR}/src/boids.c")
set(project_boids_header "${CMAKE_CURRENT_SOURCE_DIR}/src/boids.h")
set(project_main_object "${CMAKE_CURRENT_BINARY_DIR}/main.o")
set(project_boids_object "${CMAKE_CURRENT_BINARY_DIR}/boids.o")
set(project_wasm "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.wasm")
set(project_asm "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.asm")
set(project_vbin "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.vbin")
set(project_xml "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.xml")
set(project_rom "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.v32")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/rom.xml.in" "${project_xml}" @ONLY)

add_custom_command(
  OUTPUT "${project_rom}"
  # 1. Compile each freestanding C source to a relocatable wasm32 object.
  COMMAND "${CLANG_EXECUTABLE}" --target=wasm32-unknown-unknown -O2 -fno-jump-tables
    -ffreestanding -fno-builtin -nostdlib -I "${WASM2VIRCON_ROOT}/include"
    -c "${project_main_source}" -o "${project_main_object}"
  COMMAND "${CLANG_EXECUTABLE}" --target=wasm32-unknown-unknown -O2 -fno-jump-tables
    -ffreestanding -fno-builtin -nostdlib -I "${WASM2VIRCON_ROOT}/include"
    -c "${project_boids_source}" -o "${project_boids_object}"
  # 2. Link the Wasm module while retaining platform imports for wasm2vircon.
  COMMAND "${WASM_LD_EXECUTABLE}" --no-entry --export=main --allow-undefined
    "${project_main_object}" "${project_boids_object}" -o "${project_wasm}"
  # 3. Translate Wasm to Vircon32 assembly, then assemble and package the ROM.
  COMMAND "${WASM2VIRCON_EXECUTABLE}" "${project_wasm}" --entry main -o "${project_asm}"
  COMMAND "${ASSEMBLE_EXECUTABLE}" -o "${project_vbin}" "${project_asm}"
  COMMAND "${PACKROM_EXECUTABLE}" -o "${project_rom}" "${project_xml}"
  DEPENDS "${project_main_source}" "${project_boids_source}" "${project_boids_header}"
    "${WASM2VIRCON_ROOT}/include/vircon.h" "${project_xml}"
  COMMENT "Building ${project_name}.v32"
  VERBATIM)
# Keep the human-facing build target stable while project_name controls ROM
# filenames and cartridge metadata.
add_custom_target(v32 ALL DEPENDS "${project_rom}")
