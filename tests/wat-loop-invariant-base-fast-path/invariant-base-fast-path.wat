(module
  (import "env" "vircon_gpu_set_drawing_point" (func $set_point (param i32 i32)))
  (import "env" "vircon_gpu_draw_region" (func $draw))
  (memory 1)

  ;; This word holds a byte pointer to an array of 16-byte records. The loop
  ;; shape mirrors optimized container iteration: load the stable data pointer,
  ;; add a striding byte offset, then retain the intermediate field pointer.
  (data (i32.const 100) "\20\00\00\00")

  (func $entry
    (local $remaining i32)
    (local $offset i32)
    (local $field_pointer i32)
    (local.set $remaining (i32.const 4))
    (local.set $offset (i32.const 4))
    (loop $records
      (call $set_point
        (i32.trunc_sat_f32_s
          (f32.load
            (i32.add
              (local.tee $field_pointer
                (i32.add
                  (i32.load (i32.const 100))
                  (local.get $offset)))
              (i32.const -4))))
        (i32.trunc_sat_f32_s
          (f32.load (local.get $field_pointer))))
      (local.set $offset
        (i32.add (local.get $offset) (i32.const 16)))
      (call $draw)
      (br_if $records
        (local.tee $remaining
          (i32.add (local.get $remaining) (i32.const -1)))))
  )

  (export "main" (func $entry))
)
