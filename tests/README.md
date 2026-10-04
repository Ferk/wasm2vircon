# Test suite layout

Every test fixture lives in its own directory. There are no loose test source
files in this directory.

- `unit-*` contains native C unit tests for compiler helpers and V32 IR.
- `wat-*` contains focused WebAssembly profile fixtures. Each directory has a
  `testcases` metadata file and is registered as an individual CTest test.
- `c-*` contains C-to-ROM integration fixtures. Each directory is compiled,
  translated, assembled, and packaged independently by CTest.
- `dwarf` contains the DWARF input fixtures and their native fixture generator.

CMake discovers `wat-*/testcases` and `c-*/testcases` at configure time. Adding,
removing, or renaming one of those directories does not require adding its name
to a central runner list. Reconfigure and run the complete suite with:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

CTest displays every discovered fixture by name, so a failure identifies the
specific WAT or C/ROM case instead of only reporting an umbrella profile test.
Optional simulator variants are registered separately when the v32sim path and
BIOS are configured.
