(module
  (type $unary (func (param f32) (result f32)))
  (import "env" "vircon_cpu_sin" (func $sin (type $unary)))
  (import "env" "vircon_cpu_acos" (func $acos (type $unary)))
  (import "env" "vircon_cpu_log" (func $log (type $unary)))
  (import "env" "vircon_cpu_pow" (func $pow (param f32 f32) (result f32)))
  (memory 1)
  (func $twice (param $x f32) (result f32)
    (f32.mul (local.get $x) (f32.const 2)))
  (func (export "__original_main") (result i32)
    (drop
      (f32.gt
        (f32.div
          (f32.sub
            (call $sin (f32.const 1))
            (call $acos (f32.const 0.5)))
          (call $log (f32.const 2)))
        (call $pow
          (call $twice (f32.const 2))
          (f32.const 3))))
    (i32.const 0)))
