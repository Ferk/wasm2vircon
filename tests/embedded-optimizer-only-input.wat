;; The unused function references the linker global. Only the optional input
;; optimizer may remove that function; compiler-owned legalization must not
;; discard functions merely because they are currently unreachable.
(module
  (global $linker_scaffold (mut i32) (i32.const 7))
  (memory 1)
  (func $main (result i32)
    (i32.const 0))
  (func $unused
    (drop (global.get $linker_scaffold)))
  (export "main" (func $main)))
