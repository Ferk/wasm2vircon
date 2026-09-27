cmake_minimum_required(VERSION 3.20)

set(project_name "RustBoids")
project(${project_name} NONE)
set(wasm_entry "vircon_main")

find_program(CARGO_EXECUTABLE NAMES cargo REQUIRED)
find_program(WASM2VIRCON_EXECUTABLE NAMES wasm2vircon REQUIRED)
find_program(ASSEMBLE_EXECUTABLE NAMES assemble REQUIRED)
find_program(PACKROM_EXECUTABLE NAMES packrom REQUIRED)

set(project_manifest "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml")
set(project_sources "${CMAKE_CURRENT_SOURCE_DIR}/src/main.rs" "${CMAKE_CURRENT_SOURCE_DIR}/src/boids.rs"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/vircon.rs")
set(cargo_target_dir "${CMAKE_CURRENT_BINARY_DIR}/cargo-target")
set(cargo_wasm "${cargo_target_dir}/wasm32-unknown-unknown/release/rust-boids.wasm")
set(project_wasm "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.wasm")
set(project_asm "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.asm")
set(project_vbin "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.vbin")
set(project_xml "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.xml")
set(project_rom "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.v32")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/rom.xml.in" "${project_xml}" @ONLY)

add_custom_command(
  OUTPUT "${project_rom}"
  COMMAND "${CMAKE_COMMAND}" -E env "CARGO_TARGET_DIR=${cargo_target_dir}"
    "${CARGO_EXECUTABLE}" rustc --manifest-path "${project_manifest}"
    --target wasm32-unknown-unknown --release -- -C link-arg=--no-entry -C link-arg=--export=${wasm_entry}
  COMMAND "${CMAKE_COMMAND}" -E copy "${cargo_wasm}" "${project_wasm}"
  COMMAND "${WASM2VIRCON_EXECUTABLE}" "${project_wasm}" --entry "${wasm_entry}" -o "${project_asm}"
  COMMAND "${ASSEMBLE_EXECUTABLE}" -o "${project_vbin}" "${project_asm}"
  COMMAND "${PACKROM_EXECUTABLE}" -o "${project_rom}" "${project_xml}"
  DEPENDS "${project_manifest}" ${project_sources} "${project_xml}"
  COMMENT "Building ${project_name}.v32"
  VERBATIM)

add_custom_target(v32 ALL DEPENDS "${project_rom}")
