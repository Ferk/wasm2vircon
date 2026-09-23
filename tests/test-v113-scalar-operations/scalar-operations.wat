;; Exercises the v1.13 scalar additions without relying on a source frontend.
(module
  (import "env" "vircon_set_background_color"
    (func $set_background_color (param i32)))
  (import "env" "vircon_gpu_set_drawing_scale"
    (func $set_drawing_scale (param f32 f32)))
  (memory 1)
  (data (i32.const 0) "\00\00\A0\3F") ;; 1.25f, little-endian

  (func (export "main") (result i32)
    (drop (f32.eq (f32.const 1) (f32.const 1)))
    (drop (f32.ne (f32.const 1) (f32.const 2)))
    (drop (f32.ge (f32.const 2) (f32.const 1)))
    (drop (f32.neg (f32.const 1.25)))
    (drop (f32.abs (f32.const -1.25)))
    (drop (f32.floor (f32.const 1.75)))
    (drop (f32.ceil (f32.const 1.25)))

    (call $set_background_color
      (i32.extend8_s (i32.const 128)))
    (call $set_background_color
      (i32.extend16_s (i32.const 32768)))
    (call $set_background_color
      (i32.rotl (i32.const 0x12345678) (i32.const 8)))
    (call $set_background_color
      (i32.rotr (i32.const 0x12345678) (i32.const 8)))

    (f32.store (i32.const 4)
      (f32.reinterpret_i32 (i32.const 0xC0200000)))
    (call $set_background_color
      (i32.reinterpret_f32 (f32.load (i32.const 4))))
    (call $set_drawing_scale
      (f32.load (i32.const 0))
      (f32.ceil (f32.load (i32.const 0))))
    (i32.const 0)))
