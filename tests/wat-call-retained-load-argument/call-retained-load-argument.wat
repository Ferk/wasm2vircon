;; A scalar load used as the final argument must survive stores of earlier
;; call arguments. The i32.eqz/br_if makes the load eligible for retained-R1
;; lowering, which previously exposed an ABI corruption bug.
(module
  (memory 1)
  (data (i32.const 0) "\21\00\00\00")

  (func $third (param i32 i32 i32) (result i32)
    (local.get 2))

  (func $main
    (block
      (i32.eqz
        (call $third
          (i32.const 11)
          (i32.const 22)
          (i32.load (i32.const 0))))
      (br_if 0)))

  (export "main" (func $main)))
