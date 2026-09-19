#!/bin/sh
# Normalize linked Wasm into the deliberately small VirconWasm frontend profile.
set -eu

if [ "$#" -ne 2 ]; then
  echo "Usage: scripts/normalize-virconwasm.sh INPUT.wasm OUTPUT.wasm" >&2
  exit 2
fi

wasm_opt=${WASM_OPT:-wasm-opt}
if ! command -v "$wasm_opt" >/dev/null 2>&1; then
  echo "normalize-virconwasm: required tool not found: $wasm_opt" >&2
  exit 2
fi

# wasm-ld creates an unused mutable __stack_pointer global and a one-element
# funcref table for this no-start, no-indirect-call profile. These two narrow
# cleanup passes remove only unused module elements and obvious dead scaffolding;
# unlike -O2, they keep the runtime's defined functions intact.
"$wasm_opt" --remove-unused-module-elements --vacuum "$1" -o "$2"
