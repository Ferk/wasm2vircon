#!/bin/sh
# Build one freestanding C source file through the complete Vircon32 ROM path.
set -eu

usage() {
  cat <<'EOF'
Usage: scripts/build-rom.sh [--entry NAME] INPUT.c OUTPUT_DIR

Compile INPUT.c as the current freestanding VirconWasm profile, then write
INPUT's .wasm, .asm, .vbin, ROM definition XML, and .v32 files to OUTPUT_DIR.

The default entry export is __original_main. Tool paths may be overridden with
the CLANG, WASM2VIRCON, ASSEMBLE, and PACKROM environment variables; otherwise
clang, build/wasm2vircon, assemble, and packrom are used.
EOF
}

entry=__original_main
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
asm_file="$output_dir/$program_name.asm"
vbin_file="$output_dir/$program_name.vbin"
xml_file="$output_dir/$program_name.xml"
rom_file="$output_dir/$program_name.v32"

"$clang_tool" --target=wasm32-unknown-unknown -O2 -ffreestanding -fno-builtin \
  -nostdlib "$source_file" -Wl,--no-entry -Wl,--export="$entry" \
  -Wl,--allow-undefined -o "$wasm_file"
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
