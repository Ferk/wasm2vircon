#!/bin/sh
set -eu

tool=$1
clang=$2
wasm_as=$3
assemble=$4
packrom=$5
tests_dir=$6
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT HUP INT TERM

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
builder="$project_dir/scripts/build-rom.sh"

run_cases() {
  found_case=false

  for case_dir in "$tests_dir"/test-*; do
    [ -d "$case_dir" ] || continue
    case_file="$case_dir/testcases"
    [ -f "$case_file" ] || continue
    found_case=true

    source_file=
    entry=__original_main
    expectations_file=
    outcome=accept
    extra_sources=
    include_dirs=
    normalize=false
    while IFS='=' read -r key value || [ -n "$key" ]; do
      case "$key" in
        ''|'#'*) ;;
        source) source_file=$value ;;
        entry) entry=$value ;;
        expectations) expectations_file=$value ;;
        outcome) outcome=$value ;;
        extra_sources) extra_sources=$value ;;
        include_dirs) include_dirs=$value ;;
        normalize) normalize=$value ;;
        *)
          echo "unknown key in $case_file: $key" >&2
          exit 1
          ;;
      esac
    done < "$case_file"

    if [ -z "$source_file" ]; then
      echo "incomplete test case: $case_file" >&2
      exit 1
    fi

    source_path="$case_dir/$source_file"
    if [ ! -f "$source_path" ]; then
      echo "missing source for $case_file: $source_file" >&2
      exit 1
    fi

    case_name=${case_dir##*/}
    case_output="$work_dir/$case_name"
    source_name=${source_file##*/}
    program_name=${source_name%.*}

    case "$source_file:$outcome" in
      *.c:accept)
        (
          set -- "$builder" --entry "$entry"
          for extra_source in $extra_sources; do
            set -- "$@" --extra-source "$case_dir/$extra_source"
          done
          for include_dir in $include_dirs; do
            set -- "$@" --include "$case_dir/$include_dir"
          done
          if [ "$normalize" = true ]; then
            set -- "$@" --normalize
          elif [ "$normalize" != false ]; then
            echo "$case_name: normalize must be true or false" >&2
            exit 1
          fi
          WASM2VIRCON="$tool" CLANG="$clang" ASSEMBLE="$assemble" PACKROM="$packrom" "$@" "$source_path" "$case_output"
        )
        asm_file="$case_output/$program_name.asm"
        test -s "$case_output/$program_name.v32"
        ;;
      *.wat:accept)
        mkdir -p "$case_output"
        "$wasm_as" "$source_path" -o "$case_output/$program_name.wasm"
        "$tool" "$case_output/$program_name.wasm" --entry "$entry" \
          -o "$case_output/$program_name.asm"
        asm_file="$case_output/$program_name.asm"
        "$assemble" -o "$case_output/$program_name.vbin" "$asm_file"
        ;;
      *.wat:reject)
        mkdir -p "$case_output"
        "$wasm_as" "$source_path" -o "$case_output/$program_name.wasm"
        if "$tool" "$case_output/$program_name.wasm" --entry "$entry" \
          -o "$case_output/$program_name.asm" >/dev/null 2>&1; then
          echo "$case_name: invalid Wasm was accepted" >&2
          exit 1
        fi
        continue
        ;;
      *.c:reject)
        echo "$case_name: C-to-ROM cases cannot expect rejection" >&2
        exit 1
        ;;
      *)
        echo "$case_name: unsupported source/outcome combination: $source_file/$outcome" >&2
        exit 1
        ;;
    esac

    if [ -n "$expectations_file" ]; then
      expectations_path="$case_dir/$expectations_file"
      if [ ! -f "$expectations_path" ]; then
        echo "missing expectations for $case_file: $expectations_file" >&2
        exit 1
      fi
      while IFS= read -r expected || [ -n "$expected" ]; do
        case "$expected" in
          ''|'#'*) continue ;;
        esac
        if ! grep -F -- "$expected" "$asm_file" >/dev/null; then
          echo "$case_name: missing assembly fragment: $expected" >&2
          exit 1
        fi
      done < "$expectations_path"
    fi
  done

  if [ "$found_case" = false ]; then
    echo "no testcases files found under $tests_dir/test-*" >&2
    exit 1
  fi
}

run_cases
