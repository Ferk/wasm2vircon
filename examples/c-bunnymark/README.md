# BunnyMark for Vircon32

Adapted from [Joyrider3774's Bunnymark ports](https://github.com/joyrider3774/bunnymark_ports).

A port of [Timofffee's Playdate bunnymark](https://github.com/Timofffee/bunnymark-playdate),
itself raylib's `textures_bunnymark` by Ramon Santamaria, for the
[Vircon32](https://www.vircon32.com) fantasy console. Bunnies bounce around a box; you
keep adding them and watch what gives out first.


## Buttons

| Gamepad | Keyboard (emulator default) | What it does |
|---|---|---|
| A | X | +1 bunny |
| B | Z | +100 bunnies |
| X | S | +500 bunnies |
| Y | A | +1000 bunnies |
| A + B | X + Z | back to 0 bunnies |

Bunnies are added once per press, when the button is let go. Letting go of A and B
after A + B adds nothing.

## The screen

```
FPS 60  CPU 5%  GPU 16%              bunnies: 101
+------------------------------------------------+
|                bunnies bouncing                |
+------------------------------------------------+
```

- **FPS**: frames per second, counted over the last 60 of the console's frames.
- **CPU**: the CPU time the last frame's work took, as a share of one frame's 250,000
  cycles (15 MHz at 60 fps). Past 100% the work takes more than one frame.
- **GPU**: the pixels the frame asks the GPU to draw (the clear and every bunny's 16x16
  region), as a share of its budget of 9 screens a frame. Past 100% the last bunnies of
  the frame are not drawn.
- **bunnies**: how many there are, at most 100,000.

The box and the text shake while a button that adds bunnies is held, as on the Playdate.

## What it measures

Vircon32 always runs at 60 frames a second: `end_frame()` waits for the next one. So the
frame rate only shows whole fractions, 60, 30, 20 and so on, and the two percentages show
how close each part of the machine is to its limit:

1. **Move** (CPU): every bunny moves by its speed and turns around at the box's edges.
   The Playdate's code, floats and all; Vircon32's floats are full-speed hardware.
2. **Draw** (CPU and GPU): every bunny is one `draw_region_at`, the GPU drawing the
   bunny's 16x16 texture region. The call costs CPU time; the pixels cost GPU budget.

The GPU draws at most 640 x 360 x 9 = 2,073,600 pixels a frame and silently drops what
comes after. A bunny counts as its whole 16x16 region, transparent pixels included, so
about 7,200 bunnies would fill that budget; the CPU runs out first, at about 2,700. The
box and the text are drawn before the bunnies so they are never the ones dropped.

In the Vircon32 web emulator:

| Bunnies | FPS | CPU | GPU |
|---|---|---|---|
| 0 | 60 | 1% | 11% |
| 101 | 60 | 5% | 12% |
| 2,101 | 60 | 79% | 37% |
| 4,101 | 30 | 154% | 61% |

## Building

This example requires:

- CMake 3.20 or newer;
- a prebuilt `wasm2vircon` executable on `PATH`;
- Clang and `wasm-ld` with the `wasm32-unknown-unknown` target; and
- the official Vircon32 `assemble`, `png2vircon`, and `packrom` tools on
  `PATH`.

Configure and build out of tree from the repository root:

```sh
cmake -S examples/c-bunnymark -B build/c-bunnymark
cmake --build build/c-bunnymark --target v32
```

The resulting cartridge is `build/c-bunnymark/CBunnymark.v32`. If the compiler
is not on `PATH`, select it explicitly during configuration:

```sh
cmake -S examples/c-bunnymark -B build/c-bunnymark \
  -DWASM2VIRCON_EXECUTABLE=/path/to/wasm2vircon
```

Likewise, `WASM2VIRCON_INCLUDE_DIR` can name the directory containing
`vircon.h` when this example is copied outside the wasm2vircon checkout.
