# Official BeatEditor port

Upstream reference: `ConsoleSoftware/TestPrograms/Test-BeatEditor/`.

This normal-C port owns all required assets and uses only `#include
<vircon.h>`. The upstream local `bool[4][16]` matrix has become writable
`unsigned char cells[4][16]`: standard C byte addressing is intentional, and
the visible beat-editor behaviour is unchanged. It is static storage so the
restricted Wasm profile need not accept linker-generated stack globals.
