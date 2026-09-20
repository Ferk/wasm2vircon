#!/usr/bin/env bash
# Build freestanding C sources through the complete Vircon32 ROM path.
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/build-rom.sh [--entry NAME] [--rom-name NAME] [--include DIR] [--extra-source FILE]
                            [--texture PNG] [--sound WAV] [--embedded-words NAME=FILE]
                            [--tilemap NAME=TMX] [--normalize]
                            INPUT.c OUTPUT_DIR

Compile INPUT.c plus any --extra-source files as the current freestanding
VirconWasm profile, then write .wasm, .asm, .vbin, ROM definition XML, and
.v32 files to OUTPUT_DIR. --normalize retains raw linked Wasm as *.raw.wasm
and runs the supported Binaryen cleanup profile before wasm2vircon.

Each --texture and --sound is converted with the official PNG/WAV conversion
tool from PATH and is added to the generated ROM definition in option order.

--embedded-words converts a little-endian 32-bit word file into a generated C
definition named NAME, compiled with the application. --tilemap first invokes
tiled2vircon for a one-layer TMX map and then embeds its generated .vmap under
NAME. These options affect Wasm active data; they are not cartridge resources.

The default entry export is __original_main. Tool paths may be overridden with
the CLANG, WASM2VIRCON, ASSEMBLE, and PACKROM environment variables; otherwise
clang, wasm2vircon, assemble, and packrom are resolved from PATH.

--rom-name controls the basename used for all generated program artifacts and
the final NAME.v32. It defaults to the INPUT.c basename and must be supplied
without a filename extension or path.
EOF
}

entry=__original_main
rom_name=
extra_sources=()
include_dirs=()
texture_sources=()
sound_sources=()
embedded_word_specs=()
tilemap_specs=()
normalize=false
while [ "$#" -gt 0 ]; do
  case "$1" in
    --entry)
      if [ "$#" -lt 2 ]; then
        echo "build-rom: --entry requires a name" >&2
        exit 2
      fi
      entry=$2
      shift 2
      ;;
    --rom-name)
      if [ "$#" -lt 2 ]; then
        echo "build-rom: --rom-name requires a name" >&2
        exit 2
      fi
      rom_name=$2
      shift 2
      ;;
    --extra-source)
      if [ "$#" -lt 2 ]; then
        echo "build-rom: --extra-source requires a path" >&2
        exit 2
      fi
      extra_sources+=("$2")
      shift 2
      ;;
    --include)
      if [ "$#" -lt 2 ]; then
        echo "build-rom: --include requires a directory" >&2
        exit 2
      fi
      include_dirs+=("$2")
      shift 2
      ;;
    --texture)
      if [ "$#" -lt 2 ]; then
        echo "build-rom: --texture requires a PNG path" >&2
        exit 2
      fi
      texture_sources+=("$2")
      shift 2
      ;;
    --sound)
      if [ "$#" -lt 2 ]; then
        echo "build-rom: --sound requires a WAV path" >&2
        exit 2
      fi
      sound_sources+=("$2")
      shift 2
      ;;
    --embedded-words)
      if [ "$#" -lt 2 ]; then
        echo "build-rom: --embedded-words requires NAME=FILE" >&2
        exit 2
      fi
      embedded_word_specs+=("$2")
      shift 2
      ;;
    --tilemap)
      if [ "$#" -lt 2 ]; then
        echo "build-rom: --tilemap requires NAME=TMX" >&2
        exit 2
      fi
      tilemap_specs+=("$2")
      shift 2
      ;;
    --normalize)
      normalize=true
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    --)
      shift
      break
      ;;
    -*)
      echo "build-rom: unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
    *)
      break
      ;;
  esac
done

if [ "$#" -ne 2 ]; then
  usage >&2
  exit 2
fi

source_file=$1
output_dir=$2
if [ ! -f "$source_file" ]; then
  echo "build-rom: source file not found: $source_file" >&2
  exit 2
fi

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
clang_tool=${CLANG:-clang}
compiler=${WASM2VIRCON:-wasm2vircon}
assembler=${ASSEMBLE:-assemble}
rom_packer=${PACKROM:-packrom}
png_converter=${PNG2VIRCON:-png2vircon}
wav_converter=${WAV2VIRCON:-wav2vircon}
tiled_converter=${TILED2VIRCON:-tiled2vircon}
normalizer="$script_dir/normalize-virconwasm.sh"

for tool in "$clang_tool" "$compiler" "$assembler" "$rom_packer"; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "build-rom: required tool not found: $tool" >&2
    exit 2
  fi
done
if [ "${#texture_sources[@]}" -ne 0 ] && ! command -v "$png_converter" >/dev/null 2>&1; then
  echo "build-rom: required tool not found: $png_converter" >&2
  exit 2
fi
if [ "${#sound_sources[@]}" -ne 0 ] && ! command -v "$wav_converter" >/dev/null 2>&1; then
  echo "build-rom: required tool not found: $wav_converter" >&2
  exit 2
fi
if [ "${#tilemap_specs[@]}" -ne 0 ] && ! command -v "$tiled_converter" >/dev/null 2>&1; then
  echo "build-rom: required tool not found: $tiled_converter" >&2
  exit 2
fi

source_name=${source_file##*/}
program_name=${source_name%.c}
if [ "$program_name" = "$source_name" ] || [ -z "$program_name" ]; then
  echo "build-rom: input must have a .c filename: $source_file" >&2
  exit 2
fi
if [ -n "$rom_name" ]; then
  if ! [[ "$rom_name" =~ ^[A-Za-z0-9][A-Za-z0-9_-]*$ ]]; then
    echo "build-rom: --rom-name must use only letters, digits, _ or -: $rom_name" >&2
    exit 2
  fi
  program_name=$rom_name
fi

mkdir -p "$output_dir"
wasm_file="$output_dir/$program_name.wasm"
raw_wasm_file="$wasm_file"
asm_file="$output_dir/$program_name.asm"
vbin_file="$output_dir/$program_name.vbin"
xml_file="$output_dir/$program_name.xml"
rom_file="$output_dir/$program_name.v32"

# Keep source-level multi-way branches as ordinary structured branches. The
# backend deliberately has no br_table support yet; this flag is part of the
# supported frontend contract, not a test-specific workaround.
clang_flags=(--target=wasm32-unknown-unknown -O2 -fno-jump-tables -ffreestanding -fno-builtin -nostdlib)
for include_dir in "${include_dirs[@]}"; do
  if [ ! -d "$include_dir" ]; then
    echo "build-rom: include directory not found: $include_dir" >&2
    exit 2
  fi
  clang_flags+=(-I "$include_dir")
done

validate_embedded_words_spec() {
  case "$1" in
    *=*) ;;
    *) echo "build-rom: embedded word data must use NAME=FILE: $1" >&2; exit 2 ;;
  esac
  embedded_name=${1%%=*}
  embedded_file=${1#*=}
  if ! [[ "$embedded_name" =~ ^[A-Za-z_][A-Za-z0-9_]*$ ]]; then
    echo "build-rom: embedded word name is not a C identifier: $embedded_name" >&2
    exit 2
  fi
  if [ ! -f "$embedded_file" ]; then
    echo "build-rom: embedded word file not found: $embedded_file" >&2
    exit 2
  fi
  if [ $(( $(wc -c < "$embedded_file") % 4 )) -ne 0 ]; then
    echo "build-rom: embedded word file is not a whole number of 32-bit words: $embedded_file" >&2
    exit 2
  fi
}

for tilemap_spec in "${tilemap_specs[@]}"; do
  case "$tilemap_spec" in
    *=*) ;;
    *) echo "build-rom: tilemap must use NAME=TMX: $tilemap_spec" >&2; exit 2 ;;
  esac
  tilemap_name=${tilemap_spec%%=*}
  tilemap_source=${tilemap_spec#*=}
  if [ ! -f "$tilemap_source" ]; then
    echo "build-rom: tilemap source not found: $tilemap_source" >&2
    exit 2
  fi
  tilemap_output="$output_dir/tilemap-${#embedded_word_specs[@]}"
  mkdir -p "$tilemap_output"
  "$tiled_converter" "$tilemap_source" -o "$tilemap_output/"
  tilemap_files=("$tilemap_output"/*.vmap)
  if [ "${#tilemap_files[@]}" -ne 1 ] || [ ! -f "${tilemap_files[0]}" ]; then
    echo "build-rom: --tilemap currently requires exactly one generated .vmap file" >&2
    exit 2
  fi
  embedded_word_specs+=("$tilemap_name=${tilemap_files[0]}")
done

embedded_sources=()
for embedded_spec in "${embedded_word_specs[@]}"; do
  validate_embedded_words_spec "$embedded_spec"
  embedded_name=${embedded_spec%%=*}
  embedded_file=${embedded_spec#*=}
  embedded_source="$output_dir/$program_name.embedded-${#embedded_sources[@]}.c"
  {
    printf '%s\n' '/* Generated from a project-supplied little-endian word asset. */'
    od --endian=little -An -tx4 -v "$embedded_file" | awk -v name="$embedded_name" '
      BEGIN { printf "const int %s[] = {\n", name; count = 0; }
      {
        for (field = 1; field <= NF; ++field) {
          if ((count % 8) == 0) printf "    ";
          printf "(int)0x%sU", $field;
          count++;
          if ((count % 8) == 0) printf ",\n"; else printf ", ";
        }
      }
      END { if ((count % 8) != 0) printf "\n"; printf "};\n"; }
    '
  } > "$embedded_source"
  embedded_sources+=("$embedded_source")
done

# The supported normalized profile always has an explicit clang-object /
# wasm-ld boundary.  This makes one-file header-only applications behave like
# the former separately compiled runtime modules.  Keep Clang's direct link
# path for deliberately unnormalized, one-source compiler probes: it avoids
# linker scaffolding that those narrow raw-Wasm tests intentionally reject.
if [ "$normalize" = false ] && [ "${#extra_sources[@]}" -eq 0 ] && [ "${#embedded_sources[@]}" -eq 0 ]; then
  "$clang_tool" "${clang_flags[@]}" "$source_file" -Wl,--no-entry \
    -Wl,--export="$entry" -Wl,--allow-undefined -o "$raw_wasm_file"
else
  sources=("$source_file" "${extra_sources[@]}" "${embedded_sources[@]}")
  objects=()
  if [ "$normalize" = true ]; then
    raw_wasm_file="$output_dir/$program_name.raw.wasm"
  fi
  for source in "${sources[@]}"; do
    if [ ! -f "$source" ]; then
      echo "build-rom: extra source file not found: $source" >&2
      exit 2
    fi
    object_file="$output_dir/$program_name.part-${#objects[@]}.o"
    "$clang_tool" "${clang_flags[@]}" -c "$source" -o "$object_file"
    objects+=("$object_file")
  done
  wasm-ld --no-entry --export="$entry" --allow-undefined "${objects[@]}" -o "$raw_wasm_file"
fi

if [ "$normalize" = true ]; then
  "$normalizer" "$raw_wasm_file" "$wasm_file"
fi
"$compiler" "$wasm_file" --entry "$entry" -o "$asm_file"
"$assembler" -o "$vbin_file" "$asm_file"
texture_outputs=()
sound_outputs=()
for texture_source in "${texture_sources[@]}"; do
  if [ ! -f "$texture_source" ]; then
    echo "build-rom: texture source file not found: $texture_source" >&2
    exit 2
  fi
  texture_output="texture-${#texture_outputs[@]}.vtex"
  "$png_converter" "$texture_source" -o "$output_dir/$texture_output"
  texture_outputs+=("$texture_output")
done
for sound_source in "${sound_sources[@]}"; do
  if [ ! -f "$sound_source" ]; then
    echo "build-rom: sound source file not found: $sound_source" >&2
    exit 2
  fi
  sound_output="sound-${#sound_outputs[@]}.vsnd"
  SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-dummy}" "$wav_converter" "$sound_source" -o "$output_dir/$sound_output"
  sound_outputs+=("$sound_output")
done
{
  printf '%s\n' \
    '<?xml version="1.0" encoding="UTF-8" standalone="no" ?>' \
    '<rom-definition version="1.0">' \
    "  <rom type=\"cartridge\" title=\"$program_name\" version=\"1.0\" />" \
    "  <binary path=\"$program_name.vbin\" />" \
    '  <textures>'
  for texture_output in "${texture_outputs[@]}"; do
    printf '    <texture path="%s" />\n' "$texture_output"
  done
  printf '%s\n' '  </textures>' '  <sounds>'
  for sound_output in "${sound_outputs[@]}"; do
    printf '    <sound path="%s" />\n' "$sound_output"
  done
  printf '%s\n' '  </sounds>' '</rom-definition>'
} > "$xml_file"
"$rom_packer" -o "$rom_file" "$xml_file"

printf 'Built %s\n' "$rom_file"
