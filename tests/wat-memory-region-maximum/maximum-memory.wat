(module
  ;; The corrected middle RAM region permits 133 whole Wasm pages.
  (memory 133)
  (func $entry (result i32) (i32.const 0))
  (export "__original_main" (func $entry))
)
