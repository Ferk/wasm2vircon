(module
  (memory 1)

  ;; Models the nested distance comparison generated for particle/boid code:
  ;; two local.tee subtraction results are squared, added, and consumed by a
  ;; structured conditional branch.
  (func $distance_condition
    (param $ax f32) (param $ay f32) (param $bx f32) (param $by f32)
    (result i32)
    (local $dx f32) (local $dy f32)
    (block $outside
      (br_if $outside
        (i32.eqz
          (f32.lt
            (f32.add
              (f32.mul
                (local.tee $dx (f32.sub (local.get $ax) (local.get $bx)))
                (local.get $dx))
              (f32.mul
                (local.tee $dy (f32.sub (local.get $ay) (local.get $by)))
                (local.get $dy)))
            (f32.const 3600))))
      (return (i32.const 1)))
    (i32.const 0))

  (func $main (result i32)
    (call $distance_condition
      (f32.const 10) (f32.const 20) (f32.const 13) (f32.const 24)))

  (export "__original_main" (func $main)))
