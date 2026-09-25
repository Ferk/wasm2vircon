(module
  (memory 1)
  (func $entry (result i32)
    (local $source i32)
    (local $destination i32)
    ;; Create eight source bytes beginning at an unaligned byte address.
    (i32.store (i32.const 1) (i32.const 0x44332211))
    (i32.store (i32.const 5) (i32.const 0x88776655))
    (local.set $source (i32.const 1))
    (local.set $destination (i32.const 11))
    ;; This is the Zig aggregate-copy shape: a direct i64 load/store transport.
    (i64.store align=1
      (local.get $destination)
      (i64.load align=1 (local.get $source)))
    (i32.load (local.get $destination)))
  (export "__original_main" (func $entry)))
