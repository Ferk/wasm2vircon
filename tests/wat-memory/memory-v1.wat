(module
  (memory 1)
  (data (i32.const 0) "\11\22\33\44\55\66\77\88")
  (func $entry (result i32)
    (local $index i32)
    (local.set $index (i32.const 0))
    ;; Every byte lane is both read and written; the following word accesses
    ;; exercise aligned and +1/+2/+3 unaligned effective addresses.
    (i32.store8 (i32.const 0) (i32.const 160))
    (i32.store8 (i32.const 1) (i32.const 161))
    (i32.store8 (i32.const 2) (i32.const 162))
    (i32.store8 (i32.const 3) (i32.const 163))
    (local.set $index
      (i32.add
        (i32.load8_u (i32.const 0))
        (i32.load8_u (i32.const 1))))
    (if (i32.and (local.get $index) (i32.const 1))
      (then (local.set $index (i32.const 7))))
    (i32.store (i32.const 0) (i32.const 287454020))
    (i32.store align=1 (i32.const 1) (i32.const 1432778632))
    (i32.store align=1 (i32.const 2) (i32.const 2578103244))
    (i32.store align=1 (i32.const 3) (i32.const 3723427584))
    (i32.add
      (i32.load (i32.const 0))
      (i32.add
        (i32.load align=1 (i32.const 1))
        (i32.add
          (i32.load align=1 (i32.const 2))
          (i32.load align=1 (i32.const 3)))))
  )
  (export "__original_main" (func $entry))
)
