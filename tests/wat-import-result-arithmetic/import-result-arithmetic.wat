;; Ensures a value-returning platform import remains a normal operand when it
;; is nested inside ordinary integer arithmetic.
(module
  (import "env" "vircon_cpu_iabs" (func $abs (param i32) (result i32)))
  (memory 1)
  (func $accelerationFactor (param $value i32) (result i32)
    (i32.add
      (i32.div_s
        (i32.mul
          (call $abs (local.get $value))
          (i32.const 100))
        (i32.const -280))
      (i32.const 100)))
  (func $main (result i32)
    (call $accelerationFactor (i32.const -140)))
  (export "main" (func $main)))
