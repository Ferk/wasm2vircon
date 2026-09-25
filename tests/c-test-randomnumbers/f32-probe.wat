(module
  (import "env" "vircon_gpu_set_drawing_scale" (func $scale (param f32 f32)))
  (memory 1)
  (func (export "__original_main") (result i32)
    (call $scale
      (f32.const 1)
      (f32.mul (f32.convert_i32_s (i32.const 7)) (f32.const -2)))
    (i32.const 0)))
