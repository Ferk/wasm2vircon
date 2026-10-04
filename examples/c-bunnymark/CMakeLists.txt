cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------------------------
# Cartridge configuration
#
# This is the only section that normally needs editing when this file is
# copied into another freestanding C cartridge project. Paths may be absolute
# or relative to this CMakeLists.txt. Texture and sound list order determines
# the corresponding Vircon32 resource IDs, starting at zero.
# ---------------------------------------------------------------------------

set(project_name "CBunnymark")
set(project_rom_title "C BunnyMark")
set(project_rom_version "1.0")
set(project_entry "main")

set(project_sources
  src/main.c
)

# List project-owned headers so editing one triggers recompilation. The public
# vircon.h dependency is added automatically below.
set(project_headers)

# These optional lists customize compilation without changing the pipeline.
set(project_include_directories)
set(project_compile_options)
set(project_link_options)

set(project_textures
  assets/bunny.png
)

# WAV files listed here become sound-0.vsnd, sound-1.vsnd, and so on.
set(project_sounds)

project(${project_name} LANGUAGES NONE)


# ---------------------------------------------------------------------------
# Tool and public-header discovery
# ---------------------------------------------------------------------------

# Resolves one configured path relative to the directory containing this file.
function(resolve_project_path input_path output_variable)
  if(IS_ABSOLUTE "${input_path}")
    set(resolved_path "${input_path}")
  else()
    get_filename_component(
      resolved_path
      "${input_path}"
      ABSOLUTE
      BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
    )
  endif()
  set(${output_variable} "${resolved_path}" PARENT_SCOPE)
endfunction()

get_filename_component(
  WASM2VIRCON_CHECKOUT_ROOT
  "${CMAKE_CURRENT_LIST_DIR}/../.."
  ABSOLUTE
)

# Explicit cache values win. Normal users only need wasm2vircon on PATH; the
# checkout fallbacks make this example convenient for compiler contributors.
set(WASM2VIRCON_EXECUTABLE "" CACHE FILEPATH "Path to the wasm2vircon executable")
if(NOT WASM2VIRCON_EXECUTABLE)
  unset(WASM2VIRCON_EXECUTABLE CACHE)
  find_program(WASM2VIRCON_EXECUTABLE NAMES wasm2vircon)
endif()
if(NOT WASM2VIRCON_EXECUTABLE)
  unset(WASM2VIRCON_EXECUTABLE CACHE)
  find_program(
    WASM2VIRCON_EXECUTABLE
    NAMES wasm2vircon
    HINTS
      "${WASM2VIRCON_CHECKOUT_ROOT}"
      "${WASM2VIRCON_CHECKOUT_ROOT}/build"
    NO_DEFAULT_PATH
  )
endif()
if(NOT WASM2VIRCON_EXECUTABLE)
  message(FATAL_ERROR
    "wasm2vircon was not found on PATH; set WASM2VIRCON_EXECUTABLE explicitly")
endif()

set(WASM2VIRCON_INCLUDE_DIR "" CACHE PATH "Directory containing the public vircon.h header")
if(NOT WASM2VIRCON_INCLUDE_DIR)
  if(EXISTS "${WASM2VIRCON_CHECKOUT_ROOT}/include/vircon.h")
    set(WASM2VIRCON_INCLUDE_DIR "${WASM2VIRCON_CHECKOUT_ROOT}/include")
  else()
    unset(WASM2VIRCON_INCLUDE_DIR CACHE)
    find_path(WASM2VIRCON_INCLUDE_DIR NAMES vircon.h)
  endif()
endif()
if(NOT EXISTS "${WASM2VIRCON_INCLUDE_DIR}/vircon.h")
  message(FATAL_ERROR
    "vircon.h was not found; set WASM2VIRCON_INCLUDE_DIR to its containing directory")
endif()

find_program(CLANG_EXECUTABLE NAMES clang REQUIRED)
find_program(WASM_LD_EXECUTABLE NAMES wasm-ld REQUIRED)
find_program(ASSEMBLE_EXECUTABLE NAMES assemble REQUIRED)
find_program(PACKROM_EXECUTABLE NAMES packrom REQUIRED)

if(project_textures)
  find_program(PNG2VIRCON_EXECUTABLE NAMES png2vircon REQUIRED)
endif()
if(project_sounds)
  find_program(WAV2VIRCON_EXECUTABLE NAMES wav2vircon REQUIRED)
endif()


# ---------------------------------------------------------------------------
# Input and output paths
# ---------------------------------------------------------------------------

set(source_paths)
foreach(source IN LISTS project_sources)
  resolve_project_path("${source}" source_path)
  if(NOT EXISTS "${source_path}")
    message(FATAL_ERROR "C source not found: ${source_path}")
  endif()
  list(APPEND source_paths "${source_path}")
endforeach()

set(header_paths "${WASM2VIRCON_INCLUDE_DIR}/vircon.h")
foreach(header IN LISTS project_headers)
  resolve_project_path("${header}" header_path)
  if(NOT EXISTS "${header_path}")
    message(FATAL_ERROR "C header not found: ${header_path}")
  endif()
  list(APPEND header_paths "${header_path}")
endforeach()

set(include_flags -I "${WASM2VIRCON_INCLUDE_DIR}")
foreach(include_directory IN LISTS project_include_directories)
  resolve_project_path("${include_directory}" include_path)
  list(APPEND include_flags -I "${include_path}")
endforeach()

set(project_wasm "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.wasm")
set(project_asm "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.asm")
set(project_vbin "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.vbin")
set(project_xml "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.xml")
set(project_rom "${CMAKE_CURRENT_BINARY_DIR}/${project_name}.v32")


# ---------------------------------------------------------------------------
# C -> Wasm object files -> linked Wasm module
# ---------------------------------------------------------------------------

set(project_objects)
set(source_index 0)
foreach(source_path IN LISTS source_paths)
  set(object_path "${CMAKE_CURRENT_BINARY_DIR}/source-${source_index}.o")
  add_custom_command(
    OUTPUT "${object_path}"
    COMMAND
      "${CLANG_EXECUTABLE}"
      --target=wasm32-unknown-unknown
      -O2
      -ffreestanding
      -fno-builtin
      -nostdlib
      ${include_flags}
      ${project_compile_options}
      -c "${source_path}"
      -o "${object_path}"
    DEPENDS "${source_path}" ${header_paths}
    COMMENT "Compiling ${source_path} for wasm32"
    VERBATIM
  )
  list(APPEND project_objects "${object_path}")
  math(EXPR source_index "${source_index} + 1")
endforeach()

add_custom_command(
  OUTPUT "${project_wasm}"
  COMMAND
    "${WASM_LD_EXECUTABLE}"
    --no-entry
    "--export=${project_entry}"
    --allow-undefined
    ${project_link_options}
    ${project_objects}
    -o "${project_wasm}"
  DEPENDS ${project_objects}
  COMMENT "Linking ${project_name}.wasm"
  VERBATIM
)


# ---------------------------------------------------------------------------
# Wasm -> Vircon32 program
# ---------------------------------------------------------------------------

add_custom_command(
  OUTPUT "${project_asm}"
  COMMAND
    "${WASM2VIRCON_EXECUTABLE}"
    "${project_wasm}"
    --entry "${project_entry}"
    -o "${project_asm}"
  DEPENDS "${project_wasm}" "${WASM2VIRCON_EXECUTABLE}"
  COMMENT "Translating Wasm to Vircon32 assembly"
  VERBATIM
)

add_custom_command(
  OUTPUT "${project_vbin}"
  COMMAND "${ASSEMBLE_EXECUTABLE}" -o "${project_vbin}" "${project_asm}"
  DEPENDS "${project_asm}"
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

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/rom.xml.in"
  "${project_xml}"
  @ONLY
)


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
