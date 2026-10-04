(module
  (memory 1)
  (func $loads (param $pointer i32)
    (local $discard i32)
    ;; Affine offsets on opposite sides of the i32 wrap boundary must not be
    ;; treated as one enormous interval. The final offset-1 access keeps its
    ;; own check even though all three expressions share a canonical local.
    (local.set $discard
      (i32.load (i32.add (local.get $pointer) (i32.const -4))))
    (local.set $discard (i32.load offset=4 (local.get $pointer)))
    (local.set $discard (i32.load offset=1 align=1 (local.get $pointer)))
  )
  (func $entry
    (call $loads (i32.const 64)))
  (export "main" (func $entry))
)
