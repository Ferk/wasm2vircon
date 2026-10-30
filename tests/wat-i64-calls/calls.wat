;; i64 parameters/results across direct, indirect, nested and recursive calls.
(module
  (memory 1)
  (type $wide (func (param i32 i64 i32 i64 i32 f32) (result i64)))
  (table $functions 1 1 funcref)
  (elem (i32.const 0) $combine)

  (func $combine (type $wide) (param $a i32) (param $x i64) (param $b i32)
        (param $y i64) (param $c i32) (param $f f32) (result i64)
    ;; Write back to a parameter, then read both words and later parameters.
    (local.set $x (i64.add (local.get $x) (local.get $y)))
    (i64.add (local.get $x)
      (i64.extend_i32_u (i32.add (local.get $a) (i32.add (local.get $b) (local.get $c))))))

  (func $increment (param $x i64) (result i64)
    (return (i64.add (local.get $x) (i64.const 1))))

  (func $recurse (param $x i64) (param $n i32) (result i64)
    (if (i32.eqz (local.get $n)) (then (return (local.get $x))))
    (i64.add (call $recurse (call $increment (local.get $x)) (i32.sub (local.get $n) (i32.const 1)))
             (i64.const 1)))

  (func $main (result i32)
    (local $expected i64)
    (local $actual i64)
    (local.set $expected (i64.const 0x225486b8eb1d4f76))
    (local.set $actual
      (call $combine (i32.const 1) (i64.const 0x123456789abcdef0) (i32.const 2)
                     (i64.const 0x1020304050607080) (i32.const 3) (f32.const 1)))
    (if (i64.ne (local.get $actual) (local.get $expected)) (then (return (i32.const 1))))
    (local.set $actual
      (call_indirect $functions (type $wide) (i32.const 1) (i64.const 0x123456789abcdef0)
                     (i32.const 2) (i64.const 0x1020304050607080) (i32.const 3)
                     (f32.const 1) (i32.const 0)))
    (if (i64.ne (local.get $actual) (local.get $expected)) (then (return (i32.const 2))))
    (if (i64.ne (call $recurse (i64.const 0x12345678ffffffff) (i32.const 3))
                (i64.const 0x1234567900000005)) (then (return (i32.const 3))))
    (i32.const 0))
  (export "main" (func $main)))
