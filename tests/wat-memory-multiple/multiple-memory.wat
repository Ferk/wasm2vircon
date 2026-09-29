(module
  ;; VirconWasm has exactly one linear-memory mapping. This fixture verifies
  ;; that decoder metadata checks keep rejecting multi-memory modules without
  ;; relying on Binaryen's UTF-8-sensitive Wasm-text printer.
  (memory $first 1)
  (memory $second 1)
  (func $entry)
  (export "entry" (func $entry)))
