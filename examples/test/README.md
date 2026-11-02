# Example benchmarks

This optional CTest project builds the selected examples and collects their
assembly instruction counts, program sizes, and function frame sizes. It is
separate from the main compiler build and test suite.

From the repository root, configure the benchmark project and run it with
verbose output:

```sh
cmake -S examples/test -B build/example-tests
ctest --test-dir build/example-tests -V
```

`-V` displays each language's live benchmark tour lines as cases finish. CTest's
usual non-verbose output only displays the test result unless it fails. The
tour runs two counts each for Boids, BunnyMark, and Alpha Swarm; the current
counts are defined in the benchmark source. After preparing a case, the ROM
waits for the next display frame before starting its timer. Each case then
runs for at least 60 display frames, so setup cannot consume part of its
one-second measurement window.

The default selection includes C, C++, Zig, Rust, TinyGo, and AssemblyScript.
Each example requires its own language toolchain, `wasm2vircon`, and the official
`assemble` and `packrom` tools. A recent
`v32sim` and a Vircon32 BIOS enable the tour; Python 3 enables its live
per-case output. Without the simulator, its batch mode, or a BIOS, CTest still
collects static metrics. Without Python, it collects the aggregate simulator
profile without individual case lines. The simulator stops when the tour
finishes; the ROM itself returns to interactive mode.

Use CMake cache options when configuring to select other examples or tool
locations:

```sh
cmake -S examples/test -B build/example-tests \
  '-DBENCHMARK_EXAMPLES=c-boids;cpp-boids;zig-boids' \
  -DWASM2VIRCON_EXECUTABLE=/actual/path/to/wasm2vircon \
  -DV32SIM_EXECUTABLE=/actual/path/to/v32sim \
  -DV32SIM_BIOS=/actual/path/to/BiosWithoutLogo.v32
ctest --test-dir build/example-tests -V
```

Replace the example paths with files on your system. Each example's build
checks its own frontend dependencies. Set `BENCHMARK_REQUIRE_ALL=ON` to fail
when an example's required executable is missing, or set
`BENCHMARK_ENABLE_V32SIM=OFF` to collect static metrics only.

An existing build directory retains its previous `BENCHMARK_EXAMPLES` setting;
set it explicitly or use a fresh build directory to select the new default.

Results are written under `build/example-tests/results/`: `report.md` and
`summary.csv` contain the overview, `tour-results.csv` lists each language's
case results (when Python and v32sim are available), `frames.csv` lists function frame sizes,
`environment.txt` records tool identities, and `artifacts/` contains the
measured assembly, binary, ROM and simulator profile where available. The
collector replaces its `work/` and `artifacts/` subdirectories on each run.

You can also run the collector directly, without configuring CTest:

```sh
cmake -DEXAMPLES=c-boids -P examples/test/benchmark.cmake
```

That form writes to `build/optimization-baseline/` by default and accepts
`-DOUTPUT_DIR=...` to preserve multiple runs for comparison. Both forms use
the same runner and workload.

In script mode the options are `EXAMPLES` and `ENABLE_V32SIM`; the
`BENCHMARK_EXAMPLES` and `BENCHMARK_ENABLE_V32SIM` names apply to the CTest
configuration above. CTest cache settings do not carry over to a separate
`cmake -P` invocation.
