#!/bin/sh
# Exercises the non-emitting validation and raw Binaryen profile-report modes.

set -eu

tool=$1
wasm_as=$2
tests_dir=$3
work_dir=$(mktemp -d)
trap 'rm -rf "$work_dir"' EXIT HUP INT TERM

"$wasm_as" "$tests_dir/test-br-table/br-table.wat" -o "$work_dir/valid.wasm"
"$tool" --validate-only "$work_dir/valid.wasm" --entry main > "$work_dir/valid.out"
grep -F "VirconWasm v1.14 validation passed for entry 'main'" \
  "$work_dir/valid.out" >/dev/null

"$wasm_as" "$tests_dir/test-diagnostic-unnamed-opcode/unsupported-clz.wat" \
  -o "$work_dir/unsupported.wasm"
if "$tool" --validate-only "$work_dir/unsupported.wasm" \
  --entry __original_main \
  > "$work_dir/unsupported.out" 2> "$work_dir/unsupported.err"; then
  echo "--validate-only accepted i32.clz unexpectedly" >&2
  exit 1
fi
grep -F "Wasm i32.clz" "$work_dir/unsupported.err" >/dev/null

"$tool" --report-profile "$work_dir/unsupported.wasm" > "$work_dir/report.out"
grep -F "VirconWasm profile report" "$work_dir/report.out" >/dev/null
grep -F "functions: 2 (0 imports, 2 defined)" "$work_dir/report.out" >/dev/null
grep -F "i32.clz" "$work_dir/report.out" >/dev/null

if "$tool" --report-profile "$work_dir/valid.wasm" -o "$work_dir/no.asm" \
  >/dev/null 2> "$work_dir/invalid-mode.err"; then
  echo "--report-profile accepted an output path unexpectedly" >&2
  exit 1
fi
