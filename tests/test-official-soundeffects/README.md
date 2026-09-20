# Official SoundEffects port

Upstream reference: `ConsoleSoftware/TestPrograms/Test-SoundEffects/`.

This directory owns the piano texture and sound asset and does not use the
reference tree at build time. The port is normal C and uses project runtime
functions in place of the official inline-assembly headers.

The upstream's five mutable global state variables are equivalent `main`
locals because the current restricted Wasm profile rejects globals. The two
constant piano-note tables remain static active data. The original region
layout, sound-loop points, channel 15 configuration, note/volume controls,
pitch formula, pause behavior, and draw order are retained.
