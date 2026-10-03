# wasm2vircon

`wasm2vircon` is a restricted WebAssembly-to-[Vircon32](https://www.vircon32.com/)
compiler path for freestanding C programs:

```text
C source → clang wasm32 → wasm2vircon → Vircon32 assembly → .v32 ROM
```

It is designed to reuse Clang, [Binaryen](https://github.com/WebAssembly/binaryen),
and the official [Vircon32 resource, assembler, and ROM-packaging tools](https://github.com/vircon32/ComputerSoftware).
It is not a general WebAssembly runtime, a C standard library, or a replacement
for the official assembler.

`include/vircon.h` is the complete, documented public C API and is header-only:
applications need only include it. See [the C API reference document](docs/v32-c-api.md) for more details.

For an introduction to the restricted Wasm input profile, including the exact
supported instructions and deliberately unsupported WebAssembly features, see [the
Vircon32 WASM reference document](docs/v32-wasm-reference.md).

## Using `wasm2vircon` to build ROMs

Install or otherwise place a compatible `wasm2vircon` executable on `PATH`.
The rest of the requirements depend on the source-language frontend and the
build driver used by the cartridge:

- a compiler/toolchain that emits a compatible freestanding Wasm module—for
  example Clang plus `wasm-ld` for C/C++, TinyGo, Zig, or Rust;
- the official Vircon32 `assemble` and `packrom` tools, regardless of source
  language;
- `png2vircon` and `wav2vircon` only when the cartridge uses PNG textures or
  WAV sounds; and
- the build driver chosen by the project, such as CMake, Cargo, Zig's build
  system, Make, or another equivalent tool.

Each supplied example names its exact frontend and build-driver requirements
in its own README. The C/C++, TinyGo, and Rust examples use CMake; the Zig
example uses `zig build` directly.

You do **not** need Binaryen headers, `libbinaryen`, or an external `wasm-opt`
program merely to use a prebuilt compiler. A dynamically linked prebuilt
binary can additionally require the Binaryen shared library supplied with that
binary or installed by its package; a statically linked binary does not.

## Inspecting a Wasm module

Use validation without generating assembly when checking whether a module fits
the current restricted profile:

```sh
build/wasm2vircon --validate-only build/game.wasm --entry main
```

For a module that is not yet accepted, use the raw Binaryen inventory report:

```sh
build/wasm2vircon --report-profile build/game.wasm > build/game.profile.txt
```

The report lists module counts, imports, defined-function signatures and local
counts, input DWARF sections and best-available function source locations,
followed by Binaryen's complete Wasm text. It deliberately does not claim that
the module is VirconWasm-compatible; use `--validate-only` for that answer.
Unstripped frontend Wasm is accepted even when the installed Binaryen cannot
decode its DWARF directly. See [WebAssembly DWARF input](docs/dwarf.md) for the
supported line-table forms and current debugger-mapping limitations.

## Build wasm2vircon from source

### Prerequisites

To compile the `wasm2vircon` executable itself, install only:

- CMake and a C compiler;
- Binaryen development files (`binaryen-c.h` and `libbinaryen`).

Clang, `wasm-ld`, and the Vircon32 command-line tools are needed later to
build ROMs or run the complete integration tests, not to compile the compiler
binary itself.

CMake first tries `find_package(Binaryen CONFIG)` and consumes an exported
Binaryen target when one is installed. This is the preferred route because the
package supplies Binaryen's include paths and static-library dependencies.
Legacy installations that provide only `binaryen-c.h` and a `binaryen` library
continue to work through a header/library fallback. Set `Binaryen_ROOT` or
`CMAKE_PREFIX_PATH` to point CMake at a nonstandard package installation.

### Building

The project invokes the official Vircon32 command-line tools from `PATH`; it
does not use the source copies in `reference/` as build dependencies.

```sh
cmake -S . -B build
cmake --build build
```

This creates `build/wasm2vircon`. Confirm the command-line interface with:

```sh
build/wasm2vircon --help
```

## Testing

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Tests are opt-in: a normal CMake configuration builds only `wasm2vircon` and
does not discover or build the native test helpers. Enabling `BUILD_TESTING`
builds focused compiler tests and project-owned compatibility ports through the
complete ROM pipeline. Optional v32sim execution tests are supplemental to
manual desktop-emulator graphics and audio checks.

### Binaryen cleanup and linkage

Binaryen is required to decode Wasm, so wasm2vircon always runs its narrow
in-process cleanup before compiler-owned linker-artifact legalization. The
cleanup runs only `remove-unused-module-elements` and `vacuum`; it neither runs
`-O2` nor exposes Binaryen objects beyond the Wasm frontend boundary.

`WASM2VIRCON_EMBEDDED_BINARYEN` controls Binaryen linkage, not cleanup:

```sh
cmake -S . -B build -DWASM2VIRCON_EMBEDDED_BINARYEN=ON
cmake --build build
```

`AUTO` prefers static Binaryen when available, `ON` requires static linkage,
and `OFF` requires dynamic linkage. Static linkage may need a C++ runtime such
as `libstdc++` or `libc++`; wasm2vircon's own sources remain C. Neither
ordinary builds nor tests require an external `wasm-opt`.

`--skip-input-optimization` is retained as an explicit diagnostic/test mode.
It bypasses only the two Binaryen passes for that invocation; Binaryen decoding,
compiler-owned legalization, validation, lowering, and emission still run.

### Compiler-owned raw-linker legalization

After Binaryen cleanup, wasm2vircon performs a small conservative legalization
pass. It removes
unreferenced globals and inert table/element scaffolding, and accepts an
`i32`-typed loop only when its direct fallthrough tail is an unconditional
branch back to the same loop. This covers common raw `wasm-ld` frame-loop
shapes without implementing general Wasm optimization. Referenced globals,
exported globals/tables, table operations, indirect calls, ordinary
value-producing loops, and unused defined functions remain outside this cleanup
and are still validated normally.

## Supported frontend contract

The build helper deliberately uses:

```text
--target=wasm32-unknown-unknown -O2 -fno-jump-tables
-ffreestanding -fno-builtin -nostdlib
wasm-ld --no-entry --export=main --allow-undefined
```

Raw linked Wasm is accepted through compiler-owned linker-artifact
legalization. Do not feed arbitrary Wasm modules to the compiler and expect
them to work.

Most applications should continue to avoid Wasm globals. The compiler accepts
the one canonical mutable `__stack_pointer` global which normal Clang local
aggregates can make `wasm-ld` retain. This narrow ABI exception does not
support application globals or arbitrary Wasm global access.

Write the program entry point as `void main(void)`, matching the official
Vircon C convention. The example exports `main`; adapt its `wasm-ld --export`
and `wasm2vircon --entry` arguments for other frontend conventions such as an
explicitly exported Zig C-ABI function.

## Public C API at a glance

Applications include `#include <vircon.h>`. The header contains the ordinary
C implementation of every currently supported public helper, so no project
runtime sources are passed through `--extra-source`. The public groups are:

- frame and BIOS-font text: `clear_screen`, `print_at`, number printing, and
  `end_frame`;
- texture/region drawing, multiply colour, blending, scale, rotation, zoom,
  rotozoom, and BIOS line primitives;
- selected-gamepad input, frame counter, current time/date, and frame sleep;
- hardware RNG wrappers `rand` and `srand`;
- selected-channel sound playback, volume, speed, looping-sound setup, and
  automatic free-channel playback; and
- a small finite-`float` math layer: `sinf`, `cosf`, `tanf`, `asinf`,
  `acosf`, `expf`, `logf`, and `powf`.

Application code should not call raw `env.*` imports or `vircon__*` names.
They are private hardware bindings, not the stable C API.

## Important limitations

There is no hosted libc. In particular, do not assume `stdio`, files,
`printf`, locale, threads, WASI, or arbitrary compiler-builtins are
available. Use the small header-only API and add ordinary application C for
higher-level features. `print_at` is BIOS-font text rendering implemented as
normal C in `vircon.h`, not a compiler intrinsic. The supplied allocation
functions use a bounded configurable global heap. Define
`VIRCON_IMPLEMENTATION` in one application source before including `vircon.h`
when using allocation; see the C API reference for the setup and byte-addressed
configuration rules. Heap-using linked modules are accepted directly through
the same compiler invocation as other supported applications.

The current VirconWasm profile supports only the instructions demonstrated by
the included ports. It rejects, among other things, application Wasm globals and tables,
indirect calls, general i64 use, most floating-point operations,
bulk memory other than default-memory `memory.copy` and `memory.fill`, and
general libc-generated code. The sole i64 exception is a
statically addressed, four-byte-aligned `i64.store` fed directly by
`i64.const`; it is split into two native 32-bit stores for Clang's
adjacent-`int` initialization optimization. A second narrow exception accepts
a direct `i64.load` used as an `i64.store` value, for an eight-byte aggregate
copy; it is legalized as two i32 reads and two i32 writes. i64 arithmetic,
locals, parameters, results, and all other uses remain unsupported.
The profile also accepts extracting an i32 word from a direct i64 load, with an
optional constant logical right shift; this remains a frontend-only
two-i32-word legalization rather than general i64 support.
It also accepts the matching direct `local.tee` aggregate-copy shape, storing
the pair in two compiler frame slots so its low/high i32 fields can be consumed
immediately. Finally, an exact Zig computed-pair packing tree—zero-extended
low/high i32 words, with the high word shifted left by constant 32 and ORed—can
be the immediate operand of `i64.store`; it becomes ordered low/high i32
stores. Other i64 local uses and i64 arithmetic remain unsupported.
Keep applications simple and
inspect the generated normalized Wasm before adding a new dependency.
