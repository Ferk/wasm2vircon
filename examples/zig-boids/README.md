# Zig Boids

This is a self-contained Zig port of the C boids benchmark in
[`../c-boids`](../c-boids). It uses the same deliberately simple O(N²)
flocking simulation, BIOS-font status display, and no external resources.

The example starts with 50 boids. On gamepad 0, press **up** to add ten boids
and **down** to remove ten; the range is 10–150. The HUD shows the number of
Vircon frame slots consumed by the simulation step and the corresponding
`60 / update_frames` estimate.

## Build

Install Zig plus `wasm2vircon`, `assemble`, and `packrom`, with the latter
three tools available on `PATH`. Then run:

```sh
zig build
```

The ROM is written to `build/zig-boids.v32`. `zig build wasm` stops after the
freestanding Wasm input; `zig build v32` explicitly selects the default ROM
target.

`build.zig` is intentionally reusable: copy this directory, update
`project_name`, `entry_name`, and `source_path` at its top, then adjust
`rom.xml` to the new cartridge title and binary path. It uses Zig's own
filesystem API to create `build/`, so it does not rely on Unix `mkdir` or
`cp`; only the project toolchain executables themselves must be on `PATH`.

`src/vircon.zig` is a compact Zig-facing layer over the same low-level `env.*`
platform imports used by the C header. Its text and decimal helpers are normal
Zig functions, compiled through Wasm and wasm2vircon rather than implemented
as compiler intrinsics.
