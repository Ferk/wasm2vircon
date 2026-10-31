cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------------------------
# Cartridge configuration
#
# Update this section when adapting the example. Resource order determines
# texture and sound IDs. Paths may be absolute or relative to this file.
# ---------------------------------------------------------------------------

project(TinyGoBoids LANGUAGES NONE)
set(project_name "${PROJECT_NAME}")
set(project_rom_title "TinyGo Boids benchmarks")
set(project_rom_version "1.0")
set(project_entry "vircon_main")

# TinyGo builds the Go package from its module directory. All Go source files
# underneath this directory are tracked as build dependencies.
set(project_source_directory "${CMAKE_CURRENT_SOURCE_DIR}")
set(project_package ".")
set(project_tinygo_options -target wasm-unknown -gc=leaking -scheduler=none -panic=trap -no-debug -opt=1)
set(project_extra_dependencies)
set(project_textures)
set(project_sounds)

# ---------------------------------------------------------------------------
# Tool and input discovery
# ---------------------------------------------------------------------------

# Resolve a configured path relative to this cartridge's source directory.
function(resolve_project_path input_path output_variable)
  if(IS_ABSOLUTE "${input_path}")
    set(resolved_path "${input_path}")
  else()
    get_filename_component(resolved_path "${input_path}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
  set(${output_variable} "${resolved_path}" PARENT_SCOPE)
endfunction()

get_filename_component(WASM2VIRCON_CHECKOUT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(WASM2VIRCON_EXECUTABLE "" CACHE FILEPATH "Path to the wasm2vircon executable")
if(NOT WASM2VIRCON_EXECUTABLE)
  unset(WASM2VIRCON_EXECUTABLE CACHE)
  find_program(WASM2VIRCON_EXECUTABLE NAMES wasm2vircon)
endif()
if(NOT WASM2VIRCON_EXECUTABLE)
  unset(WASM2VIRCON_EXECUTABLE CACHE)
  find_program(WASM2VIRCON_EXECUTABLE NAMES wasm2vircon
    HINTS "${WASM2VIRCON_CHECKOUT_ROOT}" "${WASM2VIRCON_CHECKOUT_ROOT}/build" NO_DEFAULT_PATH)
endif()
if(NOT WASM2VIRCON_EXECUTABLE)
  message(FATAL_ERROR "wasm2vircon was not found on PATH; set WASM2VIRCON_EXECUTABLE explicitly")
endif()

find_program(TINYGO_EXECUTABLE NAMES tinygo REQUIRED)
find_program(ASSEMBLE_EXECUTABLE NAMES assemble REQUIRED)
find_program(PACKROM_EXECUTABLE NAMES packrom REQUIRED)
if(project_textures)
  find_program(PNG2VIRCON_EXECUTABLE NAMES png2vircon REQUIRED)
endif()
if(project_sounds)
  find_program(WAV2VIRCON_EXECUTABLE NAMES wav2vircon REQUIRED)
endif()

resolve_project_path("${project_source_directory}" source_directory)
if(NOT EXISTS "${source_directory}/go.mod")
  message(FATAL_ERROR "Go module not found: ${source_directory}/go.mod")
endif()
file(GLOB_RECURSE discovered_sources CONFIGURE_DEPENDS "${source_directory}/*.go")
set(project_dependencies "${source_directory}/go.mod")
if(EXISTS "${source_directory}/go.sum")
  list(APPEND project_dependencies "${source_directory}/go.sum")
endif()
foreach(source IN LISTS discovered_sources)
  # Exclude generated build-tree files when the build directory is nested in
  # the source tree. In-source builds retain all Go source files.
  if(NOT CMAKE_CURRENT_BINARY_DIR STREQUAL source_directory)
    cmake_path(IS_PREFIX CMAKE_CURRENT_BINARY_DIR "${source}" NORMALIZE is_generated)
  else()
    set(is_generated FALSE)
  endif()
  if(NOT is_generated)
    list(APPEND project_dependencies "${source}")
  endif()
endforeach()
foreach(dependency IN LISTS project_extra_dependencies)
  resolve_project_path("${dependency}" dependency_path)
  list(APPEND project_dependencies "${dependency_path}")
endforeach()

set(project_wasm "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.wasm")
set(project_asm "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.asm")
set(project_vbin "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.vbin")
set(project_xml "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.rom.xml")
set(project_rom "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.v32")

# ---------------------------------------------------------------------------
# Go -> Wasm -> Vircon32 program
# ---------------------------------------------------------------------------

add_custom_command(
  OUTPUT "${project_wasm}"
  COMMAND "${CMAKE_COMMAND}" -E chdir "${source_directory}"
    "${TINYGO_EXECUTABLE}" build ${project_tinygo_options} -o "${project_wasm}" "${project_package}"
  DEPENDS ${project_dependencies} "${TINYGO_EXECUTABLE}"
  COMMENT "Compiling ${project_name} for wasm32"
  VERBATIM
)

add_custom_command(
  OUTPUT "${project_asm}"
  COMMAND "${WASM2VIRCON_EXECUTABLE}" "${project_wasm}" --entry "${project_entry}" -o "${project_asm}"
  DEPENDS "${project_wasm}" "${WASM2VIRCON_EXECUTABLE}"
  COMMENT "Translating Wasm to Vircon32 assembly"
  VERBATIM
)

add_custom_command(
  OUTPUT "${project_vbin}"
  COMMAND "${ASSEMBLE_EXECUTABLE}" -o "${project_vbin}" "${project_asm}"
  DEPENDS "${project_asm}" "${ASSEMBLE_EXECUTABLE}"
  COMMENT "Assembling ${project_name}.vbin"
  VERBATIM
)


# ---------------------------------------------------------------------------
# PNG/WAV resources
#
# Numbered output names make their ROM order and public resource IDs explicit,
# and avoid basename collisions between assets from different directories.
# ---------------------------------------------------------------------------

set(project_texture_outputs)
set(project_texture_xml "")
set(texture_index 0)
foreach(texture IN LISTS project_textures)
  resolve_project_path("${texture}" texture_source)
  if(NOT EXISTS "${texture_source}")
    message(FATAL_ERROR "PNG texture not found: ${texture_source}")
  endif()
  set(texture_name "texture-${texture_index}.vtex")
  set(texture_output "${CMAKE_CURRENT_BINARY_DIR}/${texture_name}")
  add_custom_command(
    OUTPUT "${texture_output}"
    COMMAND "${PNG2VIRCON_EXECUTABLE}" "${texture_source}" -o "${texture_output}"
    DEPENDS "${texture_source}"
    COMMENT "Converting texture ${texture_index}: ${texture_source}"
    VERBATIM
  )
  list(APPEND project_texture_outputs "${texture_output}")
  string(APPEND project_texture_xml "    <texture path=\"${texture_name}\" />\n")
  math(EXPR texture_index "${texture_index} + 1")
endforeach()

set(project_sound_outputs)
set(project_sound_xml "")
set(sound_index 0)
foreach(sound IN LISTS project_sounds)
  resolve_project_path("${sound}" sound_source)
  if(NOT EXISTS "${sound_source}")
    message(FATAL_ERROR "WAV sound not found: ${sound_source}")
  endif()
  set(sound_name "sound-${sound_index}.vsnd")
  set(sound_output "${CMAKE_CURRENT_BINARY_DIR}/${sound_name}")
  add_custom_command(
    OUTPUT "${sound_output}"
    COMMAND
      "${CMAKE_COMMAND}" -E env SDL_AUDIODRIVER=dummy
      "${WAV2VIRCON_EXECUTABLE}" "${sound_source}" -o "${sound_output}"
    DEPENDS "${sound_source}"
    COMMENT "Converting sound ${sound_index}: ${sound_source}"
    VERBATIM
  )
  list(APPEND project_sound_outputs "${sound_output}")
  string(APPEND project_sound_xml "    <sound path=\"${sound_name}\" />\n")
  math(EXPR sound_index "${sound_index} + 1")
endforeach()

file(WRITE "${project_xml}"
  "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n"
  "<rom-definition version=\"1.0\">\n"
  "  <rom type=\"cartridge\" title=\"${project_rom_title}\" version=\"${project_rom_version}\" />\n"
  "  <binary path=\"${project_name}.vbin\" />\n"
  "  <textures>\n${project_texture_xml}  </textures>\n"
  "  <sounds>\n${project_sound_xml}  </sounds>\n"
  "</rom-definition>\n")


# ---------------------------------------------------------------------------
# Program + resources -> cartridge ROM
# ---------------------------------------------------------------------------

add_custom_command(
  OUTPUT "${project_rom}"
  COMMAND "${PACKROM_EXECUTABLE}" -o "${project_rom}" "${project_xml}"
  DEPENDS
    "${project_vbin}"
    "${project_xml}"
    ${project_texture_outputs}
    ${project_sound_outputs}
  COMMENT "Packaging ${project_name}.v32"
  VERBATIM
)

# The stable user-facing target is always `v32`, regardless of cartridge name.
add_custom_target(v32 ALL DEPENDS "${project_rom}")
