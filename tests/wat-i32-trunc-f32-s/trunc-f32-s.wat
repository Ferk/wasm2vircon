(module
  (memory 1)

  ;; Keep the operand dynamic so the backend must emit the complete trapping
  ;; conversion rather than accepting a frontend-folded constant.
  (func $convert (param $value f32) (result i32)
    (i32.trunc_f32_s (local.get $value)))

  (func (export "__original_main") (result i32)
    (call $convert (f32.const 42.75))))
