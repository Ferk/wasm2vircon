# C++ Boids

This self-contained freestanding C++ example implements the same interactive
O(N²) flocking benchmark as [`../c-boids`](../c-boids), using the public
header-only Vircon32 API. It does not use the C++ standard library, dynamic
allocation, exceptions, RTTI, or constructors with runtime side effects.

`src/main.cpp` owns the cartridge entry, input, timing, and status display.
`src/boids.cpp` owns flock state, the simulation, workload changes, and rendering.
The explicit `vircon_main` export is C-linked so the Wasm entry name remains
stable despite normal C++ name mangling.

Gamepad 0 **up** adds ten boids and **down** removes ten, with a range of
10–150. The HUD reports active boids, simulation frame slots, and the derived
`60 / update_frames` estimate.

## Build

Install `clang++`, `wasm-ld`, `wasm2vircon`, `assemble`, and `packrom` on
`PATH`, then run:

```sh
cmake -S . -B build
cmake --build build
```

The final cartridge is `build/CppBoids.v32`. To use a specific compiler build:

```sh
cmake -S . -B build -DWASM2VIRCON_EXECUTABLE=/path/to/wasm2vircon
cmake --build build --target v32
```
