(module
  (type $background_type (func (param i32)))
  (type $end_frame_type (func))
  (type $entry_type (func (result i32)))
  (import "env" "vircon_set_background_color" (func $background (type $background_type)))
  (import "env" "vircon_end_frame" (func $end_frame (type $end_frame_type)))
  (memory 1)
  (func $entry (type $entry_type)
    (loop $loop
      (call $background (i32.const 1))
      (call $end_frame)
      (br $loop)
    )
    unreachable
  )
  (export "custom_entry" (func $entry))
)
