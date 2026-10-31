# C Boids benchmark sample

This is a self-contained CMake application build. Its `CMakeLists.txt` shows
the complete basic C-to-ROM command sequence and includes the header-only
public C API. No project runtime source files are needed.

The ROM contains three resource-free workloads. **L/R** switches between
different benchmark modes; Holding **up/down** changes its count in batches
and **left/right** changes it by one.

The different benchmark modes included are:

- **Boids** uses a deliberately simple O(N²) flocking algorithm, starting at
  50 cyan squares. Count ranges from 0 to 150, in batches of 10.
- **BunnyMark** moves independent sprites, drawn in gray using the BIOS font's
  `b` glyph and GPU multiply color. Count ranges from 0 to 9999, batches of 100.
- **Alpha Swarm** draws large, overlapping translucent `v` glyphs using the
  BIOS font. They bounce below the HUD and rotate to follow their direction.
  Count ranges from 0 to 96, in batches of 5. At high counts the GPU rejects draws
  after its pixel budget is exhausted. The HUD is drawn first, above the layers,
  so it remains visible when GPU usage reaches 100%. The glyph's region is
  derived from the active BIOS font, including with the standard BIOS.

All modes use the same HUD and timing code. `FPS` counts complete update and
drawing iterations over at least 60 display frames, including time spent
waiting for the next frame. The HUD shows 60.00 until its first measured window
finishes, about one second later. `CPU` reports the previous frame's simulation, drawing, and HUD work as a
percentage of Vircon32's 250,000-cycle display-frame budget. `GPU` reports the
pixel budget used in the hardware frame sampled at the end of the previous
iteration, capped at 100%; if an iteration spans frames it is not an aggregate
of those frames.

Press **Start** at any time to run the benchmark tour. It resets each workload
and runs each case for at least 60 display frames (about one second), going
through different modes and count values. The HUD shows the current case;
the selected interactive workload resumes after the final one. A slow iteration can span multiple display
frames, so each case finishes after its current iteration and can exceed one second.

## Collect benchmark results

From the repository root, run the C benchmark and collect code size and,
when available, a v32sim instruction profile:

```sh
cmake -DEXAMPLES=c-boids -P examples/test/benchmark.cmake
```

To include the other available language examples, omit `-DEXAMPLES=c-boids`.
Each example checks its own tools. Missing executables reported during
configuration are skipped; `-DREQUIRE_ALL=ON` instead reports them as errors.
Other build failures stop the run with the example's diagnostic. Use
`-DENABLE_V32SIM=OFF` to skip simulation explicitly.

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

## Reusing the CMake build

`CMakeLists.txt` deliberately keeps cartridge-specific values in its first
section. A new C project normally only needs to change:

```cmake
set(project_name "MyGame")
set(project_rom_title "My Game")
set(project_rom_version "1.0")
set(project_entry "main")

set(project_sources
  src/main.c
  src/game.c
)

set(project_headers
  src/game.h
)

set(project_textures
  assets/player.png
  assets/world.png
)

set(project_sounds
  assets/jump.wav
  assets/music.wav
)
```

Each source is compiled independently before `wasm-ld` links the module.
Project headers are dependency entries, so editing one rebuilds the C objects.
Additional include directories and compiler/linker flags can be placed in
`project_include_directories`, `project_compile_options`, and
`project_link_options`. As with every multi-file `vircon.h` application,
exactly one source file must define `VIRCON_IMPLEMENTATION` before including
the header; the other sources include it normally.

Resources are converted incrementally with the official tools:

- textures become `texture-0.vtex`, `texture-1.vtex`, and so on;
- sounds become `sound-0.vsnd`, `sound-1.vsnd`, and so on; and
- the generated ROM XML includes exactly those converted files.

List order is significant: it is also the texture or sound ID used by the
Vircon32 API. Empty lists are valid, and their converter is not required.
Asset decoding remains outside `wasm2vircon`.
