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
if "$tool" --help | grep -F "Embedded Binaryen normalization is enabled" \
  >/dev/null; then
  if grep -F "normalizer; raw clang/wasm-ld output" "$work_dir/unsupported.err" \
    >/dev/null; then
    echo "embedded build printed an external-normalization hint" >&2
    exit 1
  fi
else
  grep -F "raw clang/wasm-ld output may need the supported cleanup profile" \
    "$work_dir/unsupported.err" >/dev/null
  grep -F "scripts/normalize-virconwasm.sh" "$work_dir/unsupported.err" \
    >/dev/null
fi

"$wasm_as" "$tests_dir/embedded-optimizer-input.wat" \
  -o "$work_dir/optimizer-input.wasm"
"$tool" --validate-only "$work_dir/optimizer-input.wasm" --entry main \
  > "$work_dir/legalized.out"
"$tool" --validate-only "$work_dir/optimizer-input.wasm" --entry main \
  --skip-input-optimization > "$work_dir/legalized-unoptimized.out"

"$wasm_as" "$tests_dir/embedded-optimizer-only-input.wat" \
  -o "$work_dir/optimizer-only-input.wasm"
if "$tool" --help | grep -F "Embedded Binaryen normalization is enabled" \
  >/dev/null; then
  "$tool" --validate-only "$work_dir/optimizer-only-input.wasm" --entry main \
    > "$work_dir/optimized.out"
  if "$tool" --validate-only "$work_dir/optimizer-only-input.wasm" --entry main \
    --skip-input-optimization > "$work_dir/unoptimized.out" \
    2> "$work_dir/unoptimized.err"; then
    echo "--skip-input-optimization did not disable the embedded optimizer" >&2
    exit 1
  fi
  grep -F "globals remain unsupported" "$work_dir/unoptimized.err" >/dev/null
else
  if "$tool" --validate-only "$work_dir/optimizer-only-input.wasm" --entry main \
    --skip-input-optimization > "$work_dir/unoptimized.out" \
    2> "$work_dir/unoptimized.err"; then
    echo "external build accepted the unsupported global unexpectedly" >&2
    exit 1
  fi
  grep -F "globals remain unsupported" "$work_dir/unoptimized.err" >/dev/null
fi

"$tool" --report-profile "$work_dir/unsupported.wasm" > "$work_dir/report.out"
grep -F "VirconWasm profile report" "$work_dir/report.out" >/dev/null
grep -F "functions: 2 (0 imports, 2 defined)" "$work_dir/report.out" >/dev/null
grep -F "i32.clz" "$work_dir/report.out" >/dev/null

if "$tool" --report-profile "$work_dir/valid.wasm" -o "$work_dir/no.asm" \
  >/dev/null 2> "$work_dir/invalid-mode.err"; then
  echo "--report-profile accepted an output path unexpectedly" >&2
  exit 1
fi
