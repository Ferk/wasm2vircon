# Zig Boids

This is a self-contained Zig port of the C boids benchmark in
[`../c-boids`](../c-boids). It uses the same deliberately simple O(N²)
flocking simulation, BIOS-font status display, and no external resources.

The example starts with 50 boids. On gamepad 0, press **up** to add ten boids
and **down** to remove ten; the range is 10–150. The HUD shows the number of
Vircon frame slots consumed by the simulation step and the corresponding
`60 / update_frames` estimate.

## Requirements

- Zig, including its bundled `wasm32-freestanding` target support;
- a prebuilt `wasm2vircon` executable on `PATH`; and
- the official Vircon32 `assemble` and `packrom` tools on `PATH`.

This example uses `zig build` directly, so it does not require CMake. It has
no external resources and does not need `png2vircon` or `wav2vircon`. It also
does not use Clang/`wasm-ld`; Binaryen development files are needed only when
building wasm2vircon itself.

## Build

From this directory, run:

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

`src/vircon.zig` is a documented Zig-facing counterpart to the public C API.
It exposes video/region drawing, BIOS-font text, input, timers/RNG, sound,
memory-card access, and the supported finite-math helpers through idiomatic
Zig names. Its text and convenience helpers are normal Zig functions compiled
through Wasm and wasm2vircon rather than compiler intrinsics. The file starts
with a short Zig/Wasm compatibility checklist covering the explicit entry
export, linear memory, static state, `std` usage, stack-pointer global, and
risky unsupported constructs.
