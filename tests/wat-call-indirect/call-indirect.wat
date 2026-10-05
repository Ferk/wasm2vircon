(module
  (type $unary (func (param i32) (result i32)))
  (type $other (func (param i32)))
  (type $writer (func (param i32 i32 i32 i32 i32)))
  (memory 1)
  (table $functions 5 5 funcref)

  ;; Slot 0 remains null. Slot 2 intentionally has the wrong type for the
  ;; unary call site and must trap if selected rather than being called.
  (elem (i32.const 1) $add_one $other $sub_one $writer)

  (func $add_one (type $unary) (param $value i32) (result i32)
    (i32.add (local.get $value) (i32.const 1)))
  (func $other (type $other) (param i32))
  (func $sub_one (type $unary) (param $value i32) (result i32)
    (i32.sub (local.get $value) (i32.const 1)))
  (func $writer (type $writer) (param i32 i32 i32 i32 i32))

  (func $argument (result i32) (i32.const 42))
  (func $selector (result i32) (i32.const 1))
  (func $dispatch (result i32)
    (call_indirect $functions (type $unary)
      (call $argument)
      (call $selector)))
  (func $invoke_writer
    (call_indirect $functions (type $writer)
      (i32.const 10)
      (i32.const 20)
      (i32.const 30)
      (i32.const 40)
      (i32.const 50)
      (i32.const 4)))
  (func $main (result i32)
    (call $invoke_writer)
    (call $dispatch))
  (export "main" (func $main)))
