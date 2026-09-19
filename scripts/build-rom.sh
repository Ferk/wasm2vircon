#!/usr/bin/env bash
# Build freestanding C sources through the complete Vircon32 ROM path.
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/build-rom.sh [--entry NAME] [--include DIR] [--extra-source FILE]
                            [--normalize] INPUT.c OUTPUT_DIR

Compile INPUT.c plus any --extra-source files as the current freestanding
VirconWasm profile, then write .wasm, .asm, .vbin, ROM definition XML, and
.v32 files to OUTPUT_DIR. --normalize retains raw linked Wasm as *.raw.wasm
and runs the supported Binaryen cleanup profile before wasm2vircon.

The default entry export is __original_main. Tool paths may be overridden with
the CLANG, WASM2VIRCON, ASSEMBLE, and PACKROM environment variables; otherwise
clang, build/wasm2vircon, assemble, and packrom are used.
EOF
}

entry=__original_main
extra_sources=()
include_dirs=()
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
project_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
clang_tool=${CLANG:-clang}
compiler=${WASM2VIRCON:-"$project_dir/build/wasm2vircon"}
assembler=${ASSEMBLE:-assemble}
rom_packer=${PACKROM:-packrom}
normalizer="$project_dir/scripts/normalize-virconwasm.sh"

for tool in "$clang_tool" "$compiler" "$assembler" "$rom_packer"; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "build-rom: required tool not found: $tool" >&2
    exit 2
  fi
done

source_name=${source_file##*/}
program_name=${source_name%.c}
if [ "$program_name" = "$source_name" ] || [ -z "$program_name" ]; then
  echo "build-rom: input must have a .c filename: $source_file" >&2
  exit 2
fi

mkdir -p "$output_dir"
wasm_file="$output_dir/$program_name.wasm"
raw_wasm_file="$wasm_file"
asm_file="$output_dir/$program_name.asm"
vbin_file="$output_dir/$program_name.vbin"
xml_file="$output_dir/$program_name.xml"
rom_file="$output_dir/$program_name.v32"

clang_flags=(--target=wasm32-unknown-unknown -O2 -ffreestanding -fno-builtin -nostdlib)
for include_dir in "${include_dirs[@]}"; do
  if [ ! -d "$include_dir" ]; then
    echo "build-rom: include directory not found: $include_dir" >&2
    exit 2
  fi
  clang_flags+=(-I "$include_dir")
done

if [ "${#extra_sources[@]}" -eq 0 ]; then
  if [ "$normalize" = true ]; then
    raw_wasm_file="$output_dir/$program_name.raw.wasm"
  fi
  "$clang_tool" "${clang_flags[@]}" "$source_file" -Wl,--no-entry \
    -Wl,--export="$entry" -Wl,--allow-undefined -o "$raw_wasm_file"
else
  sources=("$source_file" "${extra_sources[@]}")
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
printf '%s\n' \
  '<?xml version="1.0" encoding="UTF-8" standalone="no" ?>' \
  '<rom-definition version="1.0">' \
  '  <rom type="cartridge" title="wasm2vircon ROM" version="1.0" />' \
  "  <binary path=\"$program_name.vbin\" />" \
  '  <textures></textures>' \
  '  <sounds></sounds>' \
  '</rom-definition>' > "$xml_file"
"$rom_packer" -o "$rom_file" "$xml_file"

printf 'Built %s\n' "$rom_file"
