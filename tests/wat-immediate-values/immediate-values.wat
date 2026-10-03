;; Proves that scalar constants cross calls and returns without mandatory frame
;; spills, and that target integer operations consume legal immediates directly.
(module
  (import "env" "vircon_set_background_color" (func $set_background (param i32)))
  (memory 1)
  (func $float_constant (result f32)
    (f32.const 1.5))
  (func $add_constant (param $value i32) (result i32)
    (i32.add (local.get $value) (i32.const 7)))
  (func $main (result i32)
    (call $set_background (i32.const 0x123456))
    (drop (call $float_constant))
    (call $add_constant (i32.const 5)))
  (export "main" (func $main)))
