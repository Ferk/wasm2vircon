(module
  (type $background_type (func (param i32)))
  (type $end_frame_type (func))
  (type $entry_type (func (result i32)))
  (import "env" "memory" (memory 1))
  (import "env" "vircon_set_background_color" (func $background (type $background_type)))
  (import "env" "vircon_end_frame" (func $end_frame (type $end_frame_type)))
  (func $entry (type $entry_type)
    (loop $loop
      (call $background (i32.const 2113664))
      (call $end_frame)
      (br $loop)
    )
    unreachable
  )
  (export "__original_main" (func $entry))
)
