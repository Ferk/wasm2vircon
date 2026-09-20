#!/bin/sh
set -eu

tool=$1
clang=$2
wasm_as=$3
assemble=$4
packrom=$5
png2vircon=$6
wav2vircon=$7
tiled2vircon=$8
tests_dir=$9
v32sim=${10-}
v32sim_bios=${11-}
test_mode=${12-all}
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT HUP INT TERM

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
builder="$project_dir/scripts/build-rom.sh"

run_cases() {
  found_case=false
  found_sim_case=false

  case "$test_mode" in
    all|simulator) ;;
    *)
      echo "unknown test mode: $test_mode" >&2
      exit 1
      ;;
  esac

  if [ "$test_mode" = simulator ]; then
    if [ -z "$v32sim" ] || [ -z "$v32sim_bios" ]; then
      echo "simulator tests require v32sim and a BIOS path" >&2
      exit 1
    fi
    if [ ! -x "$v32sim" ] || [ ! -f "$v32sim_bios" ]; then
      echo "simulator executable or BIOS path is unavailable" >&2
      exit 1
    fi
  fi

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
    textures=
    sounds=
    embedded_words=
    tilemaps=
    xml_expectations_file=
    normalize=false
    sim_commands_file=
    sim_expectations_file=
    sim_watch_for=
    sim_stdin_file=
    while IFS='=' read -r key value || [ -n "$key" ]; do
      case "$key" in
        ''|'#'*) ;;
        source) source_file=$value ;;
        entry) entry=$value ;;
        expectations) expectations_file=$value ;;
        outcome) outcome=$value ;;
        extra_sources) extra_sources=$value ;;
        include_dirs) include_dirs=$value ;;
        textures) textures=$value ;;
        sounds) sounds=$value ;;
        embedded_words) embedded_words=$value ;;
        tilemaps) tilemaps=$value ;;
        xml_expectations) xml_expectations_file=$value ;;
        normalize) normalize=$value ;;
        sim_commands) sim_commands_file=$value ;;
        sim_expectations) sim_expectations_file=$value ;;
        sim_watch_for) sim_watch_for=$value ;;
        sim_stdin) sim_stdin_file=$value ;;
        *)
          echo "unknown key in $case_file: $key" >&2
          exit 1
          ;;
      esac
    done < "$case_file"

    if [ "$test_mode" = simulator ]; then
      [ -n "$sim_commands_file" ] || continue
      found_sim_case=true
    fi

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
          for texture in $textures; do
            set -- "$@" --texture "$case_dir/$texture"
          done
          for sound in $sounds; do
            set -- "$@" --sound "$case_dir/$sound"
          done
          for embedded_word in $embedded_words; do
            embedded_name=${embedded_word%%=*}
            embedded_file=${embedded_word#*=}
            set -- "$@" --embedded-words "$embedded_name=$case_dir/$embedded_file"
          done
          for tilemap in $tilemaps; do
            tilemap_name=${tilemap%%=*}
            tilemap_file=${tilemap#*=}
            set -- "$@" --tilemap "$tilemap_name=$case_dir/$tilemap_file"
          done
          if [ "$normalize" = true ]; then
            set -- "$@" --normalize
          elif [ "$normalize" != false ]; then
            echo "$case_name: normalize must be true or false" >&2
            exit 1
          fi
          WASM2VIRCON="$tool" CLANG="$clang" ASSEMBLE="$assemble" PACKROM="$packrom" PNG2VIRCON="$png2vircon" WAV2VIRCON="$wav2vircon" TILED2VIRCON="$tiled2vircon" "$@" "$source_path" "$case_output"
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

    if [ -n "$xml_expectations_file" ]; then
      xml_expectations_path="$case_dir/$xml_expectations_file"
      xml_file="$case_output/$program_name.xml"
      if [ ! -f "$xml_expectations_path" ]; then
        echo "missing XML expectations for $case_file: $xml_expectations_file" >&2
        exit 1
      fi
      while IFS= read -r expected || [ -n "$expected" ]; do
        case "$expected" in
          ''|'#'*) continue ;;
        esac
        if ! grep -F -- "$expected" "$xml_file" >/dev/null; then
          echo "$case_name: missing ROM XML fragment: $expected" >&2
          exit 1
        fi
      done < "$xml_expectations_path"
    fi

    if [ "$test_mode" = simulator ]; then
      if [ "$source_file" != "${source_file%.c}.c" ] || [ "$outcome" != accept ]; then
        echo "$case_name: simulator cases must be accepted C-to-ROM tests" >&2
        exit 1
      fi
      if [ -z "$sim_expectations_file" ]; then
        echo "$case_name: simulator cases require sim_expectations" >&2
        exit 1
      fi
      sim_commands_path="$case_dir/$sim_commands_file"
      sim_expectations_path="$case_dir/$sim_expectations_file"
      sim_output="$case_output/$program_name.v32sim.out"
      if [ ! -f "$sim_commands_path" ] || [ ! -f "$sim_expectations_path" ]; then
        echo "$case_name: missing simulator command or expectation file" >&2
        exit 1
      fi
      set -- "$v32sim" --no-debug --run --errorcheck --biosfile "$v32sim_bios" \
        --entry-point 0x20000000 --command-file "$sim_commands_path"
      if [ -n "$sim_watch_for" ]; then
        set -- "$@" --watch-for "$sim_watch_for"
      fi
      set -- "$@" "$case_output/$program_name.v32"
      if [ -n "$sim_stdin_file" ]; then
        sim_stdin_path="$case_dir/$sim_stdin_file"
        if [ ! -f "$sim_stdin_path" ]; then
          echo "$case_name: missing simulator stdin file" >&2
          exit 1
        fi
        "$@" < "$sim_stdin_path" > "$sim_output" 2>&1
      else
        "$@" > "$sim_output" 2>&1
      fi
      while IFS= read -r expected || [ -n "$expected" ]; do
        case "$expected" in
          ''|'#'*) continue ;;
        esac
        if ! grep -F -- "$expected" "$sim_output" >/dev/null; then
          echo "$case_name: missing simulator output fragment: $expected" >&2
          exit 1
        fi
      done < "$sim_expectations_path"
    fi
  done

  if [ "$test_mode" = all ] && [ "$found_case" = false ]; then
    echo "no testcases files found under $tests_dir/test-*" >&2
    exit 1
  fi
  if [ "$test_mode" = simulator ] && [ "$found_sim_case" = false ]; then
    echo "no simulator testcases found under $tests_dir/test-*" >&2
    exit 1
  fi
}

run_cases
