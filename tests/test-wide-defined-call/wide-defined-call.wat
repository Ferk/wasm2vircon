(module
  (memory 1)

  (func $identity (param $value i32) (result i32)
    (local.get $value))

  (func $sum6
    (param $a i32) (param $b i32) (param $c i32)
    (param $d i32) (param $e i32) (param $f i32)
    (result i32)
    (i32.add
      (i32.add (local.get $a) (local.get $b))
      (i32.add
        (i32.add (local.get $c) (local.get $d))
        (i32.add (local.get $e) (local.get $f)))))

  ;; Keep a caller local live while nested calls form the six final arguments.
  (func $caller
    (param $a i32) (param $b i32) (param $c i32)
    (param $d i32) (param $e i32) (param $f i32)
    (result i32)
    (local $saved i32)
    (local.set $saved (local.get $a))
    (call $sum6
      (local.get $saved)
      (call $identity (local.get $b))
      (call $identity (local.get $c))
      (call $identity (local.get $d))
      (call $identity (local.get $e))
      (call $identity (local.get $f))))

  (func $entry (result i32)
    (call $caller
      (i32.const 1) (i32.const 2) (i32.const 3)
      (i32.const 4) (i32.const 5) (i32.const 6)))

  (export "__original_main" (func $entry))
)
