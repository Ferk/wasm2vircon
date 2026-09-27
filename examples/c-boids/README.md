# Boidmark C sample

This is a self-contained CMake application build. Its `CMakeLists.txt` shows
the complete basic C-to-ROM command sequence and includes the header-only
public C API. No project runtime source files are needed.

## Requirements

- a prebuilt `wasm2vircon` executable on `PATH`;
- CMake;
- Clang and `wasm-ld` with the `wasm32-unknown-unknown` target; and
- the official Vircon32 `assemble` and `packrom` tools on `PATH`.

This example has no external assets, so it does not need `png2vircon` or
`wav2vircon`. It builds an application with a prebuilt compiler, so Binaryen
headers and `libbinaryen` are not required for this ROM build.

## Build

With `wasm2vircon` and the official Vircon32 tools on `PATH`, run these
commands from the repository root:

```sh
cmake -S . -B build
cmake --build build
```

The first command only configures CMake, so it creates build-system files. The
second command runs the C → Wasm → ROM pipeline. Its result is
`build/CBoids.v32`. `WASM2VIRCON_EXECUTABLE` remains available when a
particular compiler executable should be used instead of `PATH`:

```sh
cmake -S . -B build -DWASM2VIRCON_EXECUTABLE=/path/to/wasm2vircon
cmake --build build --target v32
```

This is a deliberately simple O(N²) flocking benchmark. It starts with 50
boids and draws each one as a small cyan BIOS-pixel square. Press gamepad 0's
**up** and **down** directions to add or remove ten boids; the fixed capacity
is 150 and the minimum is 10. It uses no external resources. The on-screen
frame value measures how many hardware frame slots the simulation update
occupied, before rendering the boid squares and text; a sub-frame update
therefore displays `1`. `FPS` is `60.0 / update_frames`, the resulting
simulation-update-rate estimate. A 100-boid update is intentionally far beyond
a smooth one-frame workload on Vircon32.

Change the `project_name` value near the top of `CMakeLists.txt` when copying the sample:
it controls the generated artifact names, target name, and ROM title. See
`../../include/vircon.h` for the complete currently supported public C API;
`../../docs/vircon32-c-api.md` is a supplementary reference.
