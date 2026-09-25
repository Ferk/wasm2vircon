;; Raw linker-style scaffolding plus a typed non-fallthrough frame loop.
;; This must compile with --skip-input-optimization through compiler-owned
;; legalization alone.
(module
  (global $linker_scaffold (mut i32) (i32.const 7))
  (memory 1)
  (table 1 funcref)
  (func $main (result i32)
    (loop $frame (result i32)
      (br $frame)))
  (export "main" (func $main)))
