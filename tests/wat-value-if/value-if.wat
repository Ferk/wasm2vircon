(module
  (memory 1)

  (func $choose_i32 (param $condition i32) (result i32)
    (if (result i32)
      (local.get $condition)
      (then (i32.const 11))
      (else (i32.const 22))))

  (func $choose_f32 (param $condition i32) (result f32)
    (if (result f32)
      (local.get $condition)
      (then (f32.const 1.5))
      (else (f32.const -2.25))))

  (func $nested (param $outer i32) (param $inner i32) (result i32)
    (if (result i32)
      (local.get $outer)
      (then
        (if (result i32)
          (local.get $inner)
          (then (i32.const 33))
          (else (i32.const 44))))
      (else (i32.const 55))))

  ;; A branch which returns directly is polymorphic and does not write the
  ;; merge slot; only the arm which can reach the merge needs to do so.
  (func $early_return (param $condition i32) (result i32)
    (if (result i32)
      (local.get $condition)
      (then (return (i32.const 66)))
      (else (i32.const 77))))

  (func (export "main") (result i32)
    (drop (call $choose_f32 (i32.const 0)))
    (drop (call $early_return (i32.const 0)))
    (i32.add
      (call $choose_i32 (i32.const 1))
      (i32.add
        (call $choose_i32 (i32.const 0))
        (call $nested (i32.const 1) (i32.const 0))))))
