# Vircon32 C API through wasm2vircon

This document is the application-facing contract for agents and developers
writing C programs that will be compiled through:

```text
C → Clang wasm32 → wasm2vircon → Vircon32 assembly → .v32 ROM
```

Include the project header, not an official Vircon C header:

```c
#include <vircon.h>
```

The declarations and documented definitions in `include/vircon.h` are the
source of truth. The header is self-contained: its public helpers are ordinary
C functions compiled with the application, so no project runtime `.c` files
need to be linked. Do **not** call `vircon__*` or `env.*` Wasm imports from
application code: those are private hardware-like bindings, not the public
API.

## Minimal application shape

Use a freestanding `void main(void)`, matching the official Vircon C
convention, and present each frame with `end_frame()`:

```c
#include <vircon.h>

void main(void)
{
    for (;;) {
        clear_screen(0xFF202040);
        print_at(200, 170, "Hello, Vircon32!");
        end_frame();
    }
}
```

`clear_screen` writes the Vircon32 clear colour and requests a clear.
`end_frame` maps to the hardware frame wait (`WAIT`). Colour arguments are
passed through as 32-bit Vircon colour words; use the existing ports/examples
as the convention for a desired colour rather than assuming a host graphics
library colour type.

The example CMake build exports `main`, matching the source-level entry point.
A complete minimal build is:

```sh
cmake -S . -B build
cmake --build build
cmake -S game -B game/build \
  -DWASM2VIRCON_EXECUTABLE="$PWD/build/wasm2vircon"
cmake --build game/build
```

Start an application's build from `example/CMakeLists.txt`; see the root README
for its role. This requires
`clang`, `wasm-ld`, `assemble`, and `packrom` on `PATH` (plus
`png2vircon`/`wav2vircon` for those asset types). It creates the Wasm,
assembly, binary, ROM XML, and final `.v32` in the target output directory.
`wasm-opt` is not required.

## Basic language compatibility

`vircon.h` supplies the small vocabulary that the official Vircon C compiler
normally treats as built-in:

| Name | Wasm-side definition |
| --- | --- |
| `bool` | `typedef int bool` in pre-C23 C; values use ordinary 32-bit `i32` storage. C++ uses its built-in `bool`. |
| `true`, `false` | `1` and `0` in C. C++ uses its built-in keywords. |
| `NULL` | `0`, the normal C/Wasm null pointer. |
| `pi` | Single-precision `3.1415926f`, intentionally avoiding unsupported accidental f64 arithmetic. |
| `INT_MIN`, `INT_MAX` | Signed 32-bit integer limits. |
| `blending_alpha`, `blending_add`, `blending_subtract` | GPU blend-mode values `0x20`, `0x21`, and `0x22`. |
| `bios_character_width`, `bios_character_height` | Fixed BIOS font dimensions: `10` and `20` pixels. |

The official Vircon C compiler uses `NULL` as `-1` because its source pointers
are word addresses. This toolchain uses standard byte-addressed C/Wasm
pointers, so `NULL` is deliberately zero. Do not port pointer-sentinel logic
from official C source mechanically; use `NULL` through this header instead.

## Freestanding C++ subset

The supported C++ profile uses the official LLVM libc++ headers supplied by a
Wasm C++ SDK, such as wasi-sdk. wasm2vircon does not provide or replace the C++
standard library. Native host C++ headers are unsuitable because their target
configuration and ABI do not describe wasm32.

Compile with a WASI-capable Clang/libc++ installation, but link the resulting
objects with the same freestanding `wasm-ld` path used by other frontends:

```sh
wasi-sdk/bin/clang++ --target=wasm32-wasi -std=c++20 -O2 \
  -ffreestanding -fno-builtin -fno-exceptions -fno-rtti \
  -I/path/to/wasm2vircon/include -c game.cpp -o game.o

wasm-ld --no-entry --export=vircon_main --allow-undefined \
  game.o -o game.wasm
```

Define `VIRCON_IMPLEMENTATION` before including `vircon.h` in exactly one C++
source file when the program allocates. Besides emitting the existing bounded
Wasm heap, that source then supplies `new`, `new[]`, `delete`, and `delete[]`.
Allocation failure halts the Vircon CPU; it never throws `std::bad_alloc`.
It also supplies libc++'s retained no-exceptions abort hook. Cartridge exit
destructors registered through `__cxa_atexit` are intentionally ignored because
a ROM does not return to a hosted process. Ordinary automatic objects and
`vector` elements still have their destructors run normally.

The verified subset is official `std::vector<int>` and vectors of simple game
data using construction/destruction, `reserve`, `push_back`, `clear`, `size`,
`capacity`, `empty`, `data`, iterators, and indexing. libc++ remains responsible
for these APIs. Its header-only template code and referenced archive members
are subject to normal compiler/linker dead stripping; unused parts of the
standard library do not become part of the ROM. This does not imply that every
remaining `std` facility is supported: exceptions, RTTI, threads, locale,
iostreams, filesystem, and hosted OS services remain outside the profile.

Use an explicit C ABI entry for C++ rather than a source-level `main`, which
Clang may wrap in a hosted-style `(argc, argv)` adapter:

```cpp
#define VIRCON_IMPLEMENTATION
#include <vircon.h>
#include <vector>

extern "C" void vircon_main()
{
    std::vector<int> values;
    values.push_back(42);
    for (;;) end_frame();
}
```

Export and pass `--entry vircon_main` in the ROM build. The C++ BunnyMark
example follows this shape.

## Public runtime functions

All integer parameters and results below are Wasm/Vircon `i32` values. `float`
is a single-precision `f32`; only the finite-float operations listed later are
currently supported.

### Frame and BIOS-font text

| Function | Behaviour |
| --- | --- |
| `void clear_screen(int color)` | Sets the clear colour and issues the clear-screen GPU command. |
| `void end_frame(void)` | Waits for the next Vircon frame. |
| `void print_at(int x, int y, const char *text)` | Draws a NUL-terminated BIOS-font string at pixel coordinates. |
| `void print_uint_at(int x, int y, unsigned value)` | Draws an unsigned decimal integer. |
| `void print_int_at(int x, int y, int value)` | Draws a signed decimal integer. |
| `void print_fixed_2_at(int x, int y, float value)` | Draws a signed decimal value with two fractional digits. |

`print_at` consumes **CP-1252-compatible bytes**, not UTF-8. Each nonzero byte
directly selects a BIOS glyph region. The BIOS font has 256 10×20-pixel glyphs.
`'\n'` follows the official helper: its glyph is drawn first, then the cursor
returns to the original x coordinate and advances by 20 pixels. Text rendering
saves and restores the selected texture, but otherwise expects ordinary GPU
state and does not reset colour, scale, rotation, or blending.

No `printf`, `sprintf`, general formatting, UTF-8, Unicode, or custom-font API
exists in this profile. The small documented `itoa` helper remains available.

|  	 |	0 |	1 |	2 |	3 |	4 |	5 |	6 |	7 |	8 |	9 |	A |	B |	C |	D |	E |	F |
| -- | -- |	- | - | - | - | - | - | - | - | - | - | - | - | - | - | - |
| 0_ |	  |	  |	  |	  |	  |	  |	  |	  |	  |	  |	␉|	␊|	|	␍ |	⬚ |	▯ |
| 1_ |	  |	░ |	▒ |	▓ |	█ |	─ |	\||	┘ |	┐ |	┌ |	└ |	├ |	┤ |	┴ |	┬ |	┼ |
| 2_ |	  |	! |	" |	# |	$ |	% |	& |	' |	( |	) |	* |	+ |	, |	- |	. |	/ |
| 3_ |	0 |	1 |	2 |	3 |	4 |	5 |	6 |	7 |	8 |	9 |	: |	; |	< |	= |	> |	? |
| 4_ |	@ |	A |	B |	C |	D |	E |	F |	G |	H |	I |	J |	K |	L |	M |	N |	O |
| 5_ |	P |	Q |	R |	S |	T |	U |	V |	W |	X |	Y |	Z |	[ |	\\|	] |	^ |	_ |
| 6_ |	` |	a |	b |	c |	d |	e |	f |	g |	h |	i |	j |	k |	l |	m |	n |	o |
| 7_ |	p |	q |	r |	s |	t |	u |	v |	w |	x |	y |	z |	{ |	\||	} |	~ |   |
| 8_ |	€ | 	|	‚ |	ƒ |	„ |	… |	† |	‡ |	ˆ |	‰ |	Š |	‹ |	Œ |	  |	Ž |	  |
| 9_ |	  |	‘ |	’ |	“ |	” |	• |	– |	— |	˜ |	™ |	š |	› |	œ |	  |	ž |	Ÿ |
| A_ |	  |	¡ |	¢ |	£ |	¤ |	¥ |	¦ |	§ |	¨ |	© |	ª |	« |	¬ |	  |	® |	¯ |
| B_ |	° |	± |	² |	³ |	´ |	µ |	¶ |	· |	¸ |	¹ |	º |	» |	¼ |	½ |	¾ |	¿ |
| C_ |	À |	Á |	Â |	Ã |	Ä |	Å |	Æ |	Ç |	È |	É |	Ê |	Ë |	Ì |	Í |	Î |	Ï |
| D_ |	Ð |	Ñ |	Ò |	Ó |	Ô |	Õ |	Ö |	× |	Ø |	Ù |	Ú |	Û |	Ü |	Ý |	Þ |	ß |
| E_ |	à |	á |	â |	ã |	ä |	å |	æ |	ç |	è |	é |	ê |	ë |	ì |	í |	î |	ï |
| F_ |	ð |	ñ |	ò |	ó |	ô |	õ |	ö |	÷ |	ø |	ù |	ú |	û |	ü |	ý |	þ |	ÿ |

### Video constants and colour helpers

`screen_width` is `640` and `screen_height` is `360`. The named opaque colours
from the official API are available: `color_black`, `color_white`,
`color_gray`, `color_darkgray`, `color_lightgray`, `color_red`, `color_green`,
`color_blue`, `color_yellow`, `color_magenta`, `color_cyan`, `color_orange`,
and `color_brown`.

Vircon colour words are written in **ABGR** order on a little-endian word, even
though their logical components are RGBA. The following pure helpers preserve
the official bit layout and do not clamp inputs:

```c
int make_gray(int brightness);
int make_color_rgb(int red, int green, int blue);
int make_color_rgba(int red, int green, int blue, int alpha);
int get_color_red(int color);   int get_color_green(int color);
int get_color_blue(int color);  int get_color_alpha(int color);
```

Pass components in the range `0..255`; out-of-range values have normal C
bit-operation behavior, matching the official helpers.

### Textures, regions, and drawing

Cartridge texture indices are the order in which an application's ROM XML
declares texture resources, starting at zero. The BIOS texture is selected
internally by text/primitives; application code normally uses cartridge
textures.

| Function | Behaviour |
| --- | --- |
| `void select_texture(int texture_id)` | Selects a cartridge texture. |
| `int get_selected_texture(void)` | Returns the GPU's current selected texture; this may be the BIOS texture ID `-1`. |
| `void select_region(int region_id)` | Selects a region within the selected texture. |
| `int get_selected_region(void)` | Returns the region currently selected for the current texture. |
| `void set_region_minimum(int x, int y)` | Sets selected region minimum coordinates. |
| `void set_region_maximum(int x, int y)` | Sets selected region maximum coordinates. |
| `void set_region_hotspot(int x, int y)` | Sets selected region hotspot coordinates. |
| `define_region(min_x, min_y, max_x, max_y, hotspot_x, hotspot_y)` | Defines all selected-region bounds and hotspot values. |
| `define_region_topleft(min_x, min_y, max_x, max_y)` | Defines selected-region bounds with its hotspot at its top-left corner. |
| `void draw_region(void)` | Draws the selected region at the current drawing point. |
| `void draw_region_at(int x, int y)` | Sets the drawing point and draws the selected region. |
| `void set_drawing_point(int x, int y)` | Sets the GPU drawing point only. |
| `void get_drawing_point(int *x, int *y)` | Stores the current drawing point in normal C `int` objects. |
| `void set_multiply_color(int color)` | Sets the GPU multiply colour. |
| `int get_multiply_color(void)` | Returns the current GPU multiply colour. |
| `void set_blending_mode(int mode)` | Sets a Vircon GPU blending-mode value. |
| `int get_blending_mode(void)` | Returns the current GPU blending-mode value. |
| `void set_drawing_scale(float x, float y)` | Sets typed f32 X/Y drawing scale. |
| `void get_drawing_scale(float *x, float *y)` | Stores the current drawing scale in normal C `float` objects. |
| `void set_drawing_scale_bits(int x_bits, int y_bits)` | Sets scale from raw IEEE-754 single-precision bit patterns; use only when those exact constants are wanted. |
| `void set_drawing_angle(float angle)` | Sets typed f32 drawing angle. |
| `float get_drawing_angle(void)` | Returns the current typed f32 drawing angle. |
| `void draw_region_zoomed(void)` | Draws the selected region with current point and scale. |
| `void draw_region_zoomed_at(int x, int y)` | Sets the drawing point and draws with current scale. |
| `void draw_region_rotated(void)` | Draws the selected region with current point and angle. |
| `void draw_region_rotated_at(int x, int y)` | Sets the drawing point and draws with current angle. |
| `void draw_region_rotozoomed(void)` | Draws the selected region with current point, scale, and angle. |
| `void draw_region_rotozoomed_at(int x, int y)` | Sets the drawing point and issues a rotozoom draw with current scale/angle. |

`set_drawing_scale`, `get_drawing_scale`, `set_drawing_angle`, and
`get_drawing_angle` use typed f32 values; do not replace computed values with
integer bit casts. `set_drawing_scale_bits` is only the special case for
already-known raw f32 words.

`set_blending_mode` accepts an ordinary integer; the currently exercised
official values are alpha `0x20`, add `0x21`, and subtract `0x22`.

#### Region helpers

```c
define_region(min_x, min_y, max_x, max_y, hotspot_x, hotspot_y);
define_region_topleft(min_x, min_y, max_x, max_y);
void define_region_center(int min_x, int min_y, int max_x, int max_y);
```

`define_region` and `define_region_topleft` are function-like macros because
they apply the component GPU state writes in their documented order. They
evaluate each supplied expression once and are usable as normal statements,
but are not addressable function symbols. `define_region_center` is a
header-inline function that sets the hotspot to its integer centre.

For an array of regular regions, use:

```c
struct vircon_region_matrix {
    int first_id;
    int first_min_x, first_min_y;
    int first_max_x, first_max_y;
    int first_hotspot_x, first_hotspot_y;
    int elements_x, elements_y;
    int gap;
};
void define_region_matrix(const struct vircon_region_matrix *definition);
```

Prefer a `static const struct vircon_region_matrix` definition. That keeps the
description in active data and avoids introducing an avoidable dynamic-stack
aggregate into the restricted Wasm frontend.

#### BIOS line primitives

| Function | Behaviour |
| --- | --- |
| `void draw_bios_horizontal_line(int x1, int y, int x2)` | Draws a horizontal BIOS-pixel line using the zoomed-region operation. |
| `void draw_bios_vertical_line(int x, int y1, int y2)` | Draws a vertical BIOS-pixel line using the zoomed-region operation. |

These helpers use the BIOS white-pixel region and therefore respect the
currently selected multiply colour. They intentionally leave the BIOS texture
and region selected, like their official counterparts. Re-select application
texture/region before the next application draw if needed.

### Gamepad input and timing

Call `select_gamepad(id)` before reading a controller. The selected controller
is shared GPU-style device state, so select it again if another runtime path
may have changed it.

| Function | Behaviour |
| --- | --- |
| `void select_gamepad(int id)` | Selects a gamepad, normally `0` through `3`. |
| `int get_selected_gamepad(void)` | Returns the currently selected gamepad. |
| `int gamepad_left/right/up/down(void)` | Returns the corresponding raw directional input state. Existing ports generally test `> 0` or `== 1`. |
| `int gamepad_direction_x/y(void)` | Converts raw left/right or up/down state into `-1`, `0`, or `1`. |
| `void gamepad_direction(int *x, int *y)` | Writes both converted direction values through normal C pointers. |
| `void gamepad_direction_normalized(float *x, float *y)` | Writes a unit direction vector, including diagonal normalization. |
| `int gamepad_is_connected(void)` | Returns the selected gamepad's connection state. |
| `int gamepad_button_a/b/x/y/l/r/start(void)` | Returns the selected gamepad's raw button state. |
| `int get_cycle_counter(void)` | Reads CPU cycles elapsed within the current frame; emulator timing is not guaranteed exact. |
| `int get_frame_counter(void)` | Reads the Vircon frame counter. |
| `int get_time(void)` | Reads the hardware current-time value. |
| `int get_date(void)` | Reads the hardware current-date value. |
| `void translate_time(int, time_info *)` | Splits elapsed seconds since midnight into hours, minutes, and seconds. |
| `void translate_date(int, date_info *)` | Splits the packed Vircon year/day value into calendar fields. |
| `void sleep(int frames)` | Waits by comparing frame counters and calling `end_frame()`. |

For a simple current-frame loop, prefer `end_frame()` rather than `sleep(1)`.
The time/date values are raw Vircon hardware values; this runtime does not
provide a calendar or timezone library. `frames_per_second` is `60` and
`frame_time` is the corresponding `float` duration. `time_info` and
`date_info` are ordinary C typedefs with `hours`/`minutes`/`seconds` and
`year`/`month`/`day` integer fields. `translate_date` deliberately preserves
the official helper's leap-year rule: divisible by four but not by 100.

### Random values

| Function | Behaviour |
| --- | --- |
| `int rand(void)` | Reads Vircon's current RNG value. |
| `void srand(int seed)` | Writes Vircon's current RNG value/state. |

These are small wrappers over the hardware RNG, not a hosted libc random
implementation. Range reduction, such as `rand() % 6`, is ordinary C/Wasm and
must stay within the currently supported integer subset.

### Audio

Sound indices are the order in which an application's ROM XML declares sound
resources, starting at zero. Vircon has 16 channels, indexed `0` through `15`.

| Constant | Value | Meaning |
| --- | --- | --- |
| `sound_channels` | `16` | Number of hardware sound channels. |
| `channel_stopped` | `0x40` | Selected channel is stopped. |
| `channel_paused` | `0x41` | Selected channel is paused. |
| `channel_playing` | `0x42` | Selected channel is playing. |

| Function | Behaviour |
| --- | --- |
| `void select_sound(int sound_id)` | Selects a sound resource. |
| `int get_selected_sound(void)` | Returns the currently selected sound resource. |
| `void set_sound_loop(int enabled)` | Configures the selected sound's play-with-loop flag. |
| `void set_sound_loop_start(int position)` | Sets selected sound loop start in samples. |
| `void set_sound_loop_end(int position)` | Sets selected sound loop end in samples. |
| `void select_channel(int channel_id)` | Selects a channel. |
| `int get_selected_channel(void)` | Returns the currently selected channel. |
| `void assign_channel_sound(int channel_id, int sound_id)` | Selects a channel and assigns its sound. |
| `void play_sound_in_channel(int sound_id, int channel_id)` | Selects channel, assigns sound, and starts it. |
| `void set_channel_volume(float volume)` | Sets selected-channel volume. |
| `void set_channel_speed(float speed)` | Sets selected-channel playback speed. |
| `void set_channel_position(int position)` | Sets selected-channel sample position. |
| `void set_channel_loop(int enabled)` | Sets selected-channel looping. |
| `void set_global_volume(float volume)` | Sets global SPU volume. |
| `float get_global_volume(void)` | Returns the global SPU volume multiplier. |
| `void play_channel(int channel_id)` | Selects and starts a channel. |
| `void pause_channel(int channel_id)` | Selects and pauses a channel. |
| `void stop_channel(int channel_id)` | Selects and stops a channel. |
| `int get_channel_state(int channel_id)` | Selects a channel and returns one of `channel_stopped`, `channel_paused`, or `channel_playing`. |
| `float get_channel_speed(int channel_id)` | Selects a channel and returns its playback speed. |
| `int get_channel_position(int channel_id)` | Selects a channel and returns its sample position. |
| `void pause_all_channels(void)` | Pauses every SPU channel. |
| `void stop_all_channels(void)` | Stops every SPU channel. |
| `void resume_all_channels(void)` | Resumes every SPU channel. |
| `int play_sound(int sound_id)` | Finds a stopped channel, assigns/plays the sound, returns its channel, or returns `-1` if none are stopped. |

The channel-specific query helpers select their supplied channel and leave it
selected, matching the official C helpers. Loop and channel positions are
sample offsets within the selected sound. This is direct SPU control, not a
mixer, streaming engine, or general audio library.

### Memory cards

Vircon memory cards are word-addressed. These APIs intentionally use `int`
words: C pointers remain normal byte-addressed Wasm pointers, but every card
offset and count below is a **card word**, never a byte count.

| Function | Behaviour |
| --- | --- |
| `int card_is_connected(void)` | Returns whether a card is attached. |
| `int card_read_word(int word_index)` | Reads one card word. |
| `void card_write_word(int word_index, int value)` | Writes one card word. |
| `void card_read_words(int *dst, int offset, int count)` | Copies card words into normal C `int` storage. |
| `void card_write_words(const int *src, int offset, int count)` | Copies normal C `int` storage to card words. |
| `int card_words_match(const int *expected, int offset, int count)` | Returns nonzero when all compared words match. |
| `game_signature_words` | `20`, the number of words in a standard card signature. |
| `game_signature` | Normal-C `int[20]` type for the standard 20-word card signature. |
| `void card_read_signature(game_signature *dst)` | Reads the standard signature from card words `0..19`. |
| `void card_write_signature(const game_signature *src)` | Writes the standard signature to card words `0..19`. |
| `int card_signature_matches(const game_signature *expected)` | Compares all 20 standard signature words. |
| `int card_is_empty(void)` | Returns nonzero when the standard signature contains only zero words. |
| `void card_read_data(void *dst, int offset, int count)` | Copies `count` card words into at least `count * 4` C bytes. |
| `void card_write_data(const void *src, int offset, int count)` | Packs at least `count * 4` C bytes into `count` card words. |

The card device bounds-checks its own word addresses. Applications should
first call `card_is_connected()`; out-of-range raw card accesses are target
hardware faults, not Wasm linear-memory accesses or Wasm traps.

`card_read_data` and `card_write_data` are the normal-C equivalents of the
official word-copy routines. They keep card offsets and counts in words, but
use ordinary byte-addressed C buffers and pack each word little-endian. This
preserves card contents without incorrectly treating a Wasm byte offset as a
Vircon card word address.

### Freestanding byte, string, and allocation helpers

These are ordinary header-only C helpers over normal byte-addressed Wasm
memory. They are intentionally small and do not imply a hosted libc.

| Family | Public API |
| --- | --- |
| Character predicates | `isdigit`, `isxdigit`, `isalpha`, `isascii`, `isalphanum`, `islower`, `isupper`, `isspace` |
| Character conversion | `tolower`, `toupper` |
| Byte memory | `memset`, `memcpy`, `memmove`, `memcmp` |
| Byte strings | `strlen`, `strcmp`, `strncmp`, `strcpy`, `strncpy`, `strcat`, `strncat` |
| Number text | `itoa(int, char *, int)`, `ftoa(float, char *)` |
| Page memory | `vircon_memory_size_pages()`, `vircon_memory_grow_pages(unsigned)` |
| Allocation | `malloc(vircon_size_t)`, `free(void *)`, `calloc(vircon_size_t, vircon_size_t)`, `realloc(void *, vircon_size_t)` |
| Termination | `exit(void)` |

Character and string functions use NUL-terminated `char *` byte strings;
byte comparisons use unsigned CP-1252-compatible byte values. `islower`,
`isupper`, `tolower`, and `toupper` include the Windows-1252 Latin ranges
implemented by the official header. `memcpy` requires non-overlapping regions;
`memmove` preserves overlapping byte ranges;
`strncpy` has normal C zero-padding semantics, and `strncat` always appends a
NUL terminator. Buffer capacity is always the caller's responsibility.

`itoa` accepts bases 2 through 16. Decimal output is signed; other bases print
the i32 bit pattern unsigned. `ftoa` emits a finite practical-range value with
up to five fractional digits, trims trailing fractional zeroes, and has no
locale, exponent, NaN, infinity, or precision-format options. Keep its
integral magnitude within the signed i32 range.

`exit()` lowers to Vircon `HLT`; there is no exit-status or process model.

`vircon_memory_size_pages` and `vircon_memory_grow_pages` expose the default
Wasm memory in 64 KiB pages. Growth returns the previous page count, or
`0xFFFFFFFF` on failure. It is limited to the compiler's 122-page Wasm region.

The allocation API follows the official `misc.h` shape, adapted to normal
byte-addressed C/Wasm pointers (not the 32bit words from the official Vircon C
dialect). It is a configurable global heap with a doubly linked block list:
`malloc` splits large free blocks, `free` coalesces adjacent blocks, and 
`realloc` shrinks or expands in place when possible. `calloc` checks
multiplication overflow and clears its requested bytes.

To keep all runtime source in this header while retaining one heap across an
application with multiple `.c` files, define `VIRCON_IMPLEMENTATION` before
including `vircon.h` in **exactly one** source file:

```c
#define VIRCON_IMPLEMENTATION
#include <vircon.h>
```

All other source files include `vircon.h` normally. Programs that do not use
allocation need no implementation definition.

The heap implementation has ordinary C locals, so linked applications using it
normally retain Clang's canonical Wasm `__stack_pointer` global. wasm2vircon
accepts that one canonical linker ABI global automatically.

Before the first allocation, applications may set `malloc_start_address` and
`malloc_end_address`. They are byte pointers; the end is inclusive. A null
start defaults to the initial end of Wasm memory, preserving linked static data
and the Wasm stack. A null end defaults to the last byte of the 122-page Wasm
region. Configuration is frozen after first use. Fresh heap space calls
`memory.grow` only as needed and cannot cross the configured end or target
limit. As in the official API, non-positive sizes return null and
`realloc(ptr, 0)` frees `ptr` and returns null.

### Finite math

The header provides the official Vircon finite-math names, plus conventional
`f`-suffixed aliases for the originally available float functions:

```c
float fmod(float x, float y);
int min(int x, int y);       int max(int x, int y);       int abs(int value);
float fmin(float x, float y); float fmax(float x, float y); float fabs(float value);
float floor(float value);    float ceil(float value);    float round(float value);
float sin(float value);      float cos(float value);     float tan(float value);
float asin(float value);     float acos(float value);    float atan2(float y, float x);
float sqrt(float value);     float exp(float value);     float log(float value);
float pow(float x, float y);

/* Also available: sinf, cosf, tanf, asinf, acosf, expf, logf, powf. */
```

The direct CPU mappings are `FMOD`, `IMIN`, `IMAX`, `IABS`, `FMIN`, `FMAX`,
`FABS`, `FLR`, `CEIL`, `ROUND`, `SIN`, `ACOS`, `ATAN2`, `LOG`, and `POW`.
`cos`, `tan`, `asin`, `sqrt`, and `exp` are small ordinary-C compositions over
those operations. Use finite inputs in the target instruction's documented
domain: for example, `atan2(0, 0)`, `fmod(x, 0)`, and an out-of-range inverse
trigonometric argument produce the same target hardware error as standard
Vircon C. This profile does not promise portable NaN, infinity, signed-zero,
or all hosted-libm edge-case semantics.


## Header-only implementation model

All public functionality in this document is implemented in
`include/vircon.h`. Include that one directory:

```text
--include include
```

The functions are ordinary C, not compiler intrinsics. Thin state-access and
hardware wrappers are normal `static inline` functions, allowing an optimizing
frontend to replace them with their low-level imports and avoid unnecessary
Wasm/Vircon call frames. Larger byte-memory, string, text, and formatting loops
are kept out of line to avoid code duplication and unstable widened memory
operations. `--extra-source` remains available for an application's own
additional C files, but is not needed for the supplied C API.

## Current C and Wasm limitations

This is a freestanding environment. The build profile uses `-nostdlib`,
`-ffreestanding`, and `-fno-builtin`:

- no standard C headers or hosted libc are linked automatically;
- no `printf`, `scanf`, `puts`, files, environment variables, process API,
  errno, locale, or threads; allocation is the documented bounded global heap,
  not a general reusable hosted heap;
- no WASI, system calls, dynamic loader, exceptions, or C++ runtime;
- write small ordinary-C helpers instead of relying on compiler-generated
  libc calls;
- static string/data and byte pointers work through the project’s packed
  byte-addressed Wasm-memory lowering, but raw Vircon word addresses are not
  C pointers.

All official Vircon C public-header facilities should already have a documented
Wasm-side equivalent. Heap addresses and sizes use normal C byte units rather
than the official compiler's word-addressed units. Unsupported hosted-C
features such as `printf` and files were not promised by the freestanding
Vircon C environment either.

The accepted normalized Wasm remains deliberately restricted. The current
profile supports the proven i32/f32 slice, including signed division/remainder,
`i32.shr_s`, `i32.xor`, `i32.le_s`, `i32.le_u`, and `f32.convert_i32_u`; it also supports
locals, active static data, direct defined calls, byte and i32 memory
operations, structured loops/branches, and resultless conditionals. It also
accepts the single canonical mutable `i32` `__stack_pointer` linker global
used by automatic aggregates. It accepts resultless structured `br_table`
branches, including ordinary C jump tables that lower to that form. It also accepts calls through one immutable,
statically initialized Wasm function table, with runtime trapping for invalid
or type-mismatched slots. It still rejects application globals, mutable or imported tables, f64, SIMD,
atomics, multiple/imported memories, bulk memory other than
`memory.copy`/`memory.fill`, general i64 values other than the documented
literal initializer, direct eight-byte aggregate-transfer, i32 word-extraction,
and exact computed two-i32-word aggregate-packing store forms, plus the matching
restricted two-word local aggregate transport, most
integer and floating-point operations, and unexamined libc output.
