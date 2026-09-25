(module
  ;; One page beyond the middle RAM region reserved for VirconWasm memory.
  (memory 123)
  (func $entry (result i32) (i32.const 0))
  (export "__original_main" (func $entry))
)
