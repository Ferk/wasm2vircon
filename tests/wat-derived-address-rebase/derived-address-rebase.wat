;; Reproduces Clang's address shape for adjacent fields in a statically based,
;; dynamically indexed structure. The second local.tee changes $address from
;; a scaled index into the complete structure pointer.
(module
  (type $consume_type (func (param i32 i32 f32 f32)))
  (import "env" "vircon_timer_get_frame_counter"
    (func $frame_counter (result i32)))
  (memory 3)

  (func $consume (type $consume_type))

  (func (export "main")
    (local $address i32)
    (call $consume
      (i32.load offset=138688
        (local.tee $address
          (i32.shl (call $frame_counter) (i32.const 4))))
      (i32.load offset=4
        (local.tee $address
          (i32.add (i32.const 138688) (local.get $address))))
      (f32.load offset=8 (local.get $address))
      (f32.load offset=12 (local.get $address)))))
