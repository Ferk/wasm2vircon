# Zig Boids benchmarks

This freestanding Zig example implements the Boids, BunnyMark, and Alpha
Swarm workloads from the [C benchmark](../c-boids). Workload state, rendering,
and the benchmark controller are ordinary Zig code; no textures or sounds
are required. The source files keep the workloads separate from the controller.

Use **L/R** to switch workloads, **up/down** to change the count in batches,
and **left/right** to change it by one. Holding a direction repeats using the
frame counter. **Start** runs the same six one-second cases as the C ROM and
then returns to interactive mode. The HUD shows the completed-frame FPS and
CPU/GPU usage. The optional [benchmark test](../test/README.md) collects and
compares the language tours with v32sim.

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
`project_name`, `entry_name`, and `source_path` at its top. It generates a
`.rom.xml` definition with the cartridge title and binary path. It uses Zig's own
filesystem API to create `build/`, so it does not rely on Unix `mkdir` or
`cp`; only the project toolchain executables themselves must be on `PATH`.
The Wasm retains its name section so the translator can identify Zig's
`__stack_pointer` global when the benchmark uses stack-resident state.

`src/vircon.zig` is a documented Zig-facing counterpart to the public C API.
It exposes video/region drawing, BIOS-font text, input, timers/RNG, sound,
memory-card access, and the supported finite-math helpers through idiomatic
Zig names. Its text and convenience helpers are normal Zig functions compiled
through Wasm and wasm2vircon rather than compiler intrinsics. The file starts
with a short Zig/Wasm compatibility checklist covering the explicit entry
export, linear memory, static state, `std` usage, stack-pointer global, and
risky unsupported constructs.
