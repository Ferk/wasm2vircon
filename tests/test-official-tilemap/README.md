# Official TileMap port

Upstream reference: `ConsoleSoftware/TestPrograms/Test-TileMap/`.

`assets/TileMap.tmx`, `TileSet.tsx`, and`TileSet.png` are the project-owned
map inputs; the test invokes `tiled2vircon` from `PATH` to produce one 
`.vmap`, then the generic build helper emits a C definition named `TileMap`
from its little-endian 32-bit words. The PNG used as cartridge texture is
also in `assets/` and is converted with `png2vircon`.

The source changes from the official dialect are limited to ISO C array/type
syntax, `int main(void)`, replacing `embedded` with the generated ordinary C
array, and using the project runtime. The original ten-argument
`define_region_matrix` call becomes a pointer to an equivalent ordinary C
runtime structure so it remains within the current four-argument call ABI.
