# C++ Boids benchmarks

This freestanding C++ example implements the Boids, BunnyMark, and Alpha
Swarm workloads from the [C benchmark](../c-boids). Workload state, rendering,
and the benchmark controller are ordinary C++ code; no textures or sounds
are required. The source files keep the workloads separate from the controller.

Use **L/R** to switch workloads, **up/down** to change the count in batches,
and **left/right** to change it by one. Holding a direction repeats using the
frame counter. **Start** runs the same six one-second cases as the C ROM and
then returns to interactive mode. The HUD shows the completed-frame FPS and
CPU/GPU usage. The optional [benchmark test](../test/README.md) collects and
compares the language tours with v32sim.

## Requirements

- a prebuilt `wasm2vircon` executable on `PATH`;
- CMake;
- `clang++` and `wasm-ld` with the `wasm32-unknown-unknown` target; and
- the official Vircon32 `assemble` and `packrom` tools on `PATH`.

It has no external resources, so `png2vircon` and `wav2vircon` are not needed.
Binaryen development files are needed only when building wasm2vircon itself,
not when building this C++ ROM.

## Build

From this directory, run:

```sh
cmake -S . -B build
cmake --build build
```

The final cartridge is `build/CppBoids.v32`. To use a specific compiler build:

```sh
cmake -S . -B build -DWASM2VIRCON_EXECUTABLE=/path/to/wasm2vircon
cmake --build build --target v32
```

The cartridge configuration is at the top of `CMakeLists.txt`: it lists the
sources, headers, include directories, compiler options, textures, and sounds.
The build generates `CppBoids.rom.xml` and converts any listed PNG or WAV
resources using the official tools. Those converters are needed only when the
corresponding resource list is nonempty. Set `VIRCON_CXX_COMPILER` or
`WASM2VIRCON_INCLUDE_DIR` when the defaults do not locate your compiler or
public header.
