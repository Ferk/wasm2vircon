;; An unused linker-style global is removed by the optional embedded optimizer.
;; --skip-input-optimization must leave it for ordinary VirconWasm validation.
(module
  (global $linker_scaffold (mut i32) (i32.const 7))
  (memory 1)
  (func $main (result i32)
    (i32.const 0))
  (export "main" (func $main)))
