# AssemblyScript Boids benchmarks

This AssemblyScript port implements the same Boids, BunnyMark, and Alpha
Swarm workloads as the [C benchmark](../c-boids). It uses the BIOS font and
pixel regions, so it needs no textures or sounds.

Use **L/R** to change modes, **up/down** to change the count in batches
(10 boids, 100 bunnies, or 5 alpha layers), and **left/right** to change it
by one. Holding a direction repeats after 18 display frames, every 6 frames.
The HUD shows completed-update FPS and CPU/GPU usage. Counts range from zero
to 150 boids, 9999 bunnies, or 96 alpha layers.

**Start** runs the same six cases as the other ports: 10 and 50 boids,
500 and 2500 bunnies, and 4 and 64 alpha layers. Each case starts after a
display-frame boundary and runs for at least 60 display frames. The ROM
publishes the same simulator checkpoints and then resumes interactive mode.
It can be measured by the shared [benchmark collector](../test/README.md).

## Requirements and build

- Node.js 20 or newer and npm 10 or newer;
- a prebuilt `wasm2vircon` on `PATH`;
- the official Vircon32 `assemble` and `packrom` tools on `PATH`.

From this directory:

```sh
npm ci
npm run build
```

The cartridge is `build/as-boids.v32`. `npm run build:wasm` stops after the
Wasm input. AssemblyScript is pinned in `package.json` and `package-lock.json`;
no global `asc` installation is needed. The frontend configuration is in
`asconfig.json`; the Node build script uses the local AssemblyScript compiler
API, then runs the translator, assembler, and packager. Its filesystem and
process operations work without Unix shell utilities, including on Windows.

Set `WASM2VIRCON_EXECUTABLE`, `ASSEMBLE_EXECUTABLE`, or `PACKROM_EXECUTABLE` to
override the corresponding tool. Pass `--out-dir` to select an output directory:

```sh
npm run build -- --out-dir build-custom
npm run build:wasm -- --out-dir build-custom
```

## AssemblyScript and VirconWasm

`src/index.ts` exports `vircon_main`. The workload modules use `f32`
arithmetic and `@unmanaged` records backed by `memory.data` fixed storage.
The controller and counters also live in linear memory; mutable top-level
Wasm globals are outside the translator's supported profile. All resets run
inside the entry function. Static UTF-16 string literals supply the ASCII HUD
without allocating formatted strings.

The [compiler configuration](https://www.assemblyscript.org/compiler.html)
selects the stub runtime, fixed four-page memory, and no exported memory.
The benchmark allocates no managed objects, grows no memory, and needs no
garbage collector, JavaScript host, WASI, or runtime imports such as `abort`.
[`src/vircon.ts`](src/vircon.ts) is a self-contained, documented Vircon32 API:
copy it into another project and import it using `import * as v from "./vircon"`.
It includes drawing, input, time, sound, memory-card access, RAM inspection,
finite math and BIOS-font text. Its ordinary AssemblyScript helpers use narrow
`env.vircon_*` hardware imports. Text accepts UTF-16 strings, maps representable
characters to CP-1252 BIOS glyphs, and replaces unsupported characters with
`?`; string literals require no allocation. Multi-field outputs use caller-owned
unmanaged records to avoid heap allocation:

```ts
import * as v from "./vircon";

const point = changetype<v.Point>(memory.data(offsetof<v.Point>()));

export function vircon_main(): void {
  v.selectGamepad(0);
  v.gamepadDirection(point);
  v.clearScreen(v.COLOR_BLACK);
  v.printAt(10, 10, "Hello, Vircon!");
  v.endFrame();
}
```

Memory-card offsets and counts are measured in 32-bit card words; buffer
addresses passed to card helpers are byte-addressed Wasm `usize` values and
must have room for the requested data. `ramReadWord` uses a Vircon word address,
not a Wasm byte address. Hardware selections (texture, channel, gamepad, and
region) are shared state; the documented helpers say when they change it.
The example exports only its entry function. Debug names are retained
to make translator diagnostics useful. Keep explicit `f32` types and static
storage when extending this example; ordinary JavaScript `number`, dynamic
arrays, heap allocation, and string formatting can introduce unsupported
`f64` operations or runtime globals/imports.

To collect this example alone from the repository root:

```sh
cmake -S examples/test -B build/as-benchmark -DBENCHMARK_EXAMPLES=as-boids
ctest --test-dir build/as-benchmark -V
```
