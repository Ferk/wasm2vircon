(module
  (memory 1)

  ;; Exercise every possible byte lane. Offset 3 crosses a Vircon32 word
  ;; boundary under the packed byte-to-word mapping.
  (func $round_trip (param $address i32) (param $value i32) (result i32)
    (i32.store16 (local.get $address) (local.get $value))
    (i32.add
      (i32.load16_u (local.get $address))
      (i32.add
        (i32.load16_s (local.get $address))
        (i32.load8_s (local.get $address)))))

  (func $main (result i32)
    (i32.add
      (call $round_trip (i32.const 0) (i32.const 0x000080ff))
      (i32.add
        (call $round_trip (i32.const 1) (i32.const 0x00007f01))
        (i32.add
          (call $round_trip (i32.const 2) (i32.const 0x0000ff02))
          (call $round_trip (i32.const 3) (i32.const 0x00008103))))))

  (export "main" (func $main)))
