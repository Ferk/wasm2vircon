(module
  ;; 8,000,000 bytes in the middle RAM region permits 122 whole Wasm pages.
  (memory 122)
  (func $entry (result i32) (i32.const 0))
  (export "__original_main" (func $entry))
)
