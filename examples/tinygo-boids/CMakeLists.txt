cmake_minimum_required(VERSION 3.20)

# Change this one value when copying the sample into a new TinyGo cartridge.
set(project_name "TinyGoBoids")
project(${project_name} NONE)

# TinyGo's wasm-unknown target is freestanding and retains only the explicit
# Wasm imports declared in main.go. The options avoid scheduler/GC facilities
# that are unnecessary for this allocation-free cartridge.
set(tinygo_target "wasm-unknown")
set(wasm_entry "vircon_main")

find_program(TINYGO_EXECUTABLE NAMES tinygo REQUIRED)
find_program(WASM2VIRCON_EXECUTABLE NAMES wasm2vircon REQUIRED)
find_program(ASSEMBLE_EXECUTABLE NAMES assemble REQUIRED)
find_program(PACKROM_EXECUTABLE NAMES packrom REQUIRED)

set(project_main_source "${CMAKE_CURRENT_SOURCE_DIR}/main.go")
set(project_boids_source "${CMAKE_CURRENT_SOURCE_DIR}/boids.go")
set(project_module "${CMAKE_CURRENT_SOURCE_DIR}/go.mod")
set(project_wasm "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.wasm")
set(project_asm "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.asm")
set(project_vbin "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.vbin")
set(project_xml "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.xml")
set(project_rom "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.v32")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/rom.xml.in" "${project_xml}" @ONLY)

add_custom_command(
  OUTPUT "${project_rom}"
  # 1. Produce a freestanding Wasm module with direct env.* imports.
  COMMAND "${CMAKE_COMMAND}" -E chdir "${CMAKE_CURRENT_SOURCE_DIR}"
    "${TINYGO_EXECUTABLE}" build -target "${tinygo_target}"
    -gc=leaking -scheduler=none -panic=trap -no-debug -opt=1
    -o "${project_wasm}" .
  # 2. Translate Wasm, assemble it, and package the cartridge.
  COMMAND "${WASM2VIRCON_EXECUTABLE}" "${project_wasm}" --entry "${wasm_entry}" -o "${project_asm}"
  COMMAND "${ASSEMBLE_EXECUTABLE}" -o "${project_vbin}" "${project_asm}"
  COMMAND "${PACKROM_EXECUTABLE}" -o "${project_rom}" "${project_xml}"
  DEPENDS "${project_main_source}" "${project_boids_source}" "${project_module}" "${project_xml}"
  # TinyGo runs beside go.mod through cmake -E chdir; the remaining stages use
  # the binary directory so packrom resolves the configured relative VBIN.
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  COMMENT "Building ${project_name}.v32"
  VERBATIM)

# Keep the human-facing build target stable while project_name controls ROM
# filenames and cartridge metadata.
add_custom_target(v32 ALL DEPENDS "${project_rom}")
