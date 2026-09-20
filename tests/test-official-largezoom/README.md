# Official LargeZoom port

Upstream reference: `ConsoleSoftware/TestPrograms/Test-LargeZoom/`.

The directory owns `TextureCheckers.png` and is buildable without the
reference tree. The port replaces inline-assembly headers with the project
runtime and rewrites the pointer-mutating `clamp_int` helper as an equivalent
return-value function, avoiding a Wasm linker stack global that has no source
semantic value here. It intentionally retains the upstream's two down-button
checks.

The official word-addressed `strcpy`/`itoa`/`strcat` construction is replaced
with the established byte-oriented text and numeric runtime. The displayed
labels and values are retained; their fixed label positions are a small
presentation adaptation rather than a general string-formatting library.
