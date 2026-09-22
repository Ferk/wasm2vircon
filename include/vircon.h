#ifndef VIRCON_H
#define VIRCON_H

/*
 * Vircon32 C API for wasm2vircon.
 *
 * Include this one header from a freestanding C program compiled for wasm32.
 * It is header-only: the public helpers below are ordinary `static` C code,
 * and their `vircon__*` calls are private hardware-like Wasm imports.  No
 * runtime .c files need to be carried, compiled, or linked by applications.
 *
 * This is not a hosted libc.  It has no stdio, printf, malloc, files, string
 * library, or UTF-8 support.  Use only the documented public names below;
 * `vircon__*` names are implementation details of the current platform ABI.
 * The header is for Clang wasm32.  The host fallback merely permits parsing;
 * it does not supply hardware implementations.
 */

#if defined(__wasm__)
#define VIRCON__IMPORT(name) __attribute__((import_module("env"), import_name(name)))
#else
#define VIRCON__IMPORT(name)
#endif

/* Private platform bindings. Not application API. */
void vircon__set_background_color(int color) VIRCON__IMPORT("vircon_set_background_color");
void vircon__end_frame(void) VIRCON__IMPORT("vircon_end_frame");
int vircon__gpu_get_selected_texture(void) VIRCON__IMPORT("vircon_gpu_get_selected_texture");
void vircon__gpu_select_texture(int value) VIRCON__IMPORT("vircon_gpu_select_texture");
int vircon__gpu_get_selected_region(void) VIRCON__IMPORT("vircon_gpu_get_selected_region");
void vircon__gpu_select_region(int value) VIRCON__IMPORT("vircon_gpu_select_region");
void vircon__gpu_set_drawing_point(int x, int y) VIRCON__IMPORT("vircon_gpu_set_drawing_point");
void vircon__gpu_draw_region(void) VIRCON__IMPORT("vircon_gpu_draw_region");
void vircon__gpu_set_region_minimum(int x, int y) VIRCON__IMPORT("vircon_gpu_set_region_minimum");
void vircon__gpu_set_region_maximum(int x, int y) VIRCON__IMPORT("vircon_gpu_set_region_maximum");
void vircon__gpu_set_region_hotspot(int x, int y) VIRCON__IMPORT("vircon_gpu_set_region_hotspot");
void vircon__gpu_set_multiply_color(int value) VIRCON__IMPORT("vircon_gpu_set_multiply_color");
void vircon__gpu_set_active_blending(int value) VIRCON__IMPORT("vircon_gpu_set_active_blending");
void vircon__gpu_set_drawing_scale_bits(int x, int y) VIRCON__IMPORT("vircon_gpu_set_drawing_scale_bits");
void vircon__gpu_set_drawing_scale(float x, float y) VIRCON__IMPORT("vircon_gpu_set_drawing_scale");
void vircon__gpu_set_drawing_angle(float value) VIRCON__IMPORT("vircon_gpu_set_drawing_angle");
void vircon__gpu_draw_region_zoomed(void) VIRCON__IMPORT("vircon_gpu_draw_region_zoomed");
void vircon__gpu_draw_region_rotated(void) VIRCON__IMPORT("vircon_gpu_draw_region_rotated");
void vircon__gpu_draw_region_rotozoomed(void) VIRCON__IMPORT("vircon_gpu_draw_region_rotozoomed");
float vircon__cpu_sin(float value) VIRCON__IMPORT("vircon_cpu_sin");
float vircon__cpu_acos(float value) VIRCON__IMPORT("vircon_cpu_acos");
float vircon__cpu_log(float value) VIRCON__IMPORT("vircon_cpu_log");
float vircon__cpu_pow(float x, float y) VIRCON__IMPORT("vircon_cpu_pow");
void vircon__input_select_gamepad(int value) VIRCON__IMPORT("vircon_input_select_gamepad");
int vircon__input_gamepad_left(void) VIRCON__IMPORT("vircon_input_gamepad_left");
int vircon__input_gamepad_right(void) VIRCON__IMPORT("vircon_input_gamepad_right");
int vircon__input_gamepad_up(void) VIRCON__IMPORT("vircon_input_gamepad_up");
int vircon__input_gamepad_down(void) VIRCON__IMPORT("vircon_input_gamepad_down");
int vircon__input_gamepad_connected(void) VIRCON__IMPORT("vircon_input_gamepad_connected");
int vircon__input_gamepad_button_a(void) VIRCON__IMPORT("vircon_input_gamepad_button_a");
int vircon__input_gamepad_button_b(void) VIRCON__IMPORT("vircon_input_gamepad_button_b");
int vircon__input_gamepad_button_x(void) VIRCON__IMPORT("vircon_input_gamepad_button_x");
int vircon__input_gamepad_button_y(void) VIRCON__IMPORT("vircon_input_gamepad_button_y");
int vircon__input_gamepad_button_l(void) VIRCON__IMPORT("vircon_input_gamepad_button_l");
int vircon__input_gamepad_button_r(void) VIRCON__IMPORT("vircon_input_gamepad_button_r");
int vircon__input_gamepad_button_start(void) VIRCON__IMPORT("vircon_input_gamepad_button_start");
int vircon__timer_get_frame_counter(void) VIRCON__IMPORT("vircon_timer_get_frame_counter");
int vircon__timer_get_current_time(void) VIRCON__IMPORT("vircon_timer_get_current_time");
int vircon__timer_get_current_date(void) VIRCON__IMPORT("vircon_timer_get_current_date");
int vircon__rng_get_current_value(void) VIRCON__IMPORT("vircon_rng_get_current_value");
void vircon__rng_set_current_value(int value) VIRCON__IMPORT("vircon_rng_set_current_value");
int vircon__memcard_is_connected(void) VIRCON__IMPORT("vircon_memcard_is_connected");
int vircon__memcard_read_word(int word_index) VIRCON__IMPORT("vircon_memcard_read_word");
void vircon__memcard_write_word(int word_index, int value) VIRCON__IMPORT("vircon_memcard_write_word");
void vircon__spu_select_channel(int value) VIRCON__IMPORT("vircon_spu_select_channel");
void vircon__spu_select_sound(int value) VIRCON__IMPORT("vircon_spu_select_sound");
void vircon__spu_set_sound_play_with_loop(int value) VIRCON__IMPORT("vircon_spu_set_sound_play_with_loop");
void vircon__spu_set_sound_loop_start(int value) VIRCON__IMPORT("vircon_spu_set_sound_loop_start");
void vircon__spu_set_sound_loop_end(int value) VIRCON__IMPORT("vircon_spu_set_sound_loop_end");
void vircon__spu_set_channel_assigned_sound(int value) VIRCON__IMPORT("vircon_spu_set_channel_assigned_sound");
void vircon__spu_play_selected_channel(void) VIRCON__IMPORT("vircon_spu_play_selected_channel");
void vircon__spu_pause_selected_channel(void) VIRCON__IMPORT("vircon_spu_pause_selected_channel");
void vircon__spu_set_channel_volume(float value) VIRCON__IMPORT("vircon_spu_set_channel_volume");
void vircon__spu_set_channel_speed(float value) VIRCON__IMPORT("vircon_spu_set_channel_speed");
void vircon__spu_set_channel_loop_enabled(int value) VIRCON__IMPORT("vircon_spu_set_channel_loop_enabled");
void vircon__spu_set_global_volume(float value) VIRCON__IMPORT("vircon_spu_set_global_volume");
int vircon__spu_get_channel_state(void) VIRCON__IMPORT("vircon_spu_get_channel_state");

/* Keep this header-only library as separate ordinary C functions.  Besides
 * avoiding code duplication at every call site, this deliberately preserves
 * the narrow, tested VirconWasm shape of the former separately compiled
 * runtime.  It is not a compiler intrinsic. */
#if defined(__clang__)
#pragma clang attribute push(__attribute__((noinline)), apply_to = function)
#endif

/* Frame and BIOS-font text -------------------------------------------------
 * clear_screen uses a 32-bit Vircon colour word (for example 0xFF202040).
 * end_frame maps to Vircon WAIT. */
static inline void clear_screen(int color) { vircon__set_background_color(color); }
static inline void end_frame(void) { vircon__end_frame(); }

/* Draw a NUL-terminated CP-1252 byte string using the 10x20 BIOS font.
 * Text is software, not a compiler intrinsic or a hardware command.  Each
 * nonzero byte selects its glyph region in BIOS texture -1. A newline glyph
 * is drawn first, then x resets and y advances by 20. The selected texture is
 * restored afterwards; colour, scale, rotation and blending are unchanged. */
static void vircon__print_at(int x, int y, const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;
    const int initial_x = x;
    const int previous_texture = vircon__gpu_get_selected_texture();
    vircon__gpu_select_texture(-1);
    while (*cursor != 0) {
        int glyph = *cursor;
        vircon__gpu_select_region(glyph);
        vircon__gpu_set_drawing_point(x, y);
        vircon__gpu_draw_region();
        x += 10;
        if (glyph == '\n') { x = initial_x; y += 20; }
        ++cursor;
    }
    vircon__gpu_select_texture(previous_texture);
}
static inline void print_at(int x, int y, const char *text)
{ vircon__print_at(x, y, text); }

/* Small decimal display helpers. They are not printf and intentionally have
 * no locale, width, precision or general formatting support. */
static int vircon__print_digits(int x, int y, unsigned value)
{
    static char digit_text[2] = {'0', 0};
    unsigned quotient = value / 10u;
    if (quotient != 0) x = vircon__print_digits(x, y, quotient);
    digit_text[0] = (char)('0' + value - quotient * 10u);
    print_at(x, y, digit_text);
    return x + 10;
}
static inline void print_uint_at(int x, int y, unsigned value)
{ (void)vircon__print_digits(x, y, value); }
/* Valid for normal practical display values; INT_MIN is not special-cased. */
static inline void print_int_at(int x, int y, int value)
{
    if (value < 0) { print_at(x, y, "-"); x += 10; value = -value; }
    print_uint_at(x, y, (unsigned)value);
}
static inline void print_fixed_2_at(int x, int y, float value)
{
    static char fraction[3] = {'0', '0', 0};
    int hundredths = (int)(value * 100.0f);
    unsigned magnitude;
    if (hundredths < 0) { print_at(x, y, "-"); x += 10; hundredths = -hundredths; }
    magnitude = (unsigned)hundredths;
    print_uint_at(x, y, magnitude / 100u);
    print_at(x + 20, y, ".");
    fraction[0] = (char)('0' + (magnitude / 10u) % 10u);
    fraction[1] = (char)('0' + magnitude % 10u);
    print_at(x + 30, y, fraction);
}

/* Texture, regions and drawing --------------------------------------------
 * Texture IDs are --texture argument order; region IDs are application-set.
 * These calls change selected GPU state. */
static inline void select_texture(int id) { vircon__gpu_select_texture(id); }
/* Returns the texture currently selected in GPU state, including BIOS texture -1. */
static inline int get_selected_texture(void) { return vircon__gpu_get_selected_texture(); }
static inline void select_region(int id) { vircon__gpu_select_region(id); }
/* Returns the region currently selected for the current texture. */
static inline int get_selected_region(void) { return vircon__gpu_get_selected_region(); }
static inline void set_region_minimum(int x, int y) { vircon__gpu_set_region_minimum(x, y); }
static inline void set_region_maximum(int x, int y) { vircon__gpu_set_region_maximum(x, y); }
static inline void set_region_hotspot(int x, int y) { vircon__gpu_set_region_hotspot(x, y); }
static inline void set_drawing_point(int x, int y) { vircon__gpu_set_drawing_point(x, y); }
static inline void set_multiply_color(int color) { vircon__gpu_set_multiply_color(color); }
/* Tested blend mode words: alpha 0x20, add 0x21, subtract 0x22. */
static inline void set_blending_mode(int mode) { vircon__gpu_set_active_blending(mode); }
/* Use this typed f32 form for calculated scale values. */
static inline void set_drawing_scale(float x, float y) { vircon__gpu_set_drawing_scale(x, y); }
/* For already-known IEEE-754 f32 bits only; calculated values use the typed form. */
static inline void set_drawing_scale_bits(int x_bits, int y_bits)
{ vircon__gpu_set_drawing_scale_bits(x_bits, y_bits); }
static inline void set_drawing_angle(float angle) { vircon__gpu_set_drawing_angle(angle); }
static inline void draw_region(void) { vircon__gpu_draw_region(); }
static inline void draw_region_zoomed(void) { vircon__gpu_draw_region_zoomed(); }
static inline void draw_region_at(int x, int y)
{ set_drawing_point(x, y); vircon__gpu_draw_region(); }
static inline void draw_region_zoomed_at(int x, int y)
{ set_drawing_point(x, y); vircon__gpu_draw_region_zoomed(); }
static inline void draw_region_rotated(void) { vircon__gpu_draw_region_rotated(); }
static inline void draw_region_rotated_at(int x, int y)
{ set_drawing_point(x, y); vircon__gpu_draw_region_rotated(); }
static inline void draw_region_rotozoomed(void) { vircon__gpu_draw_region_rotozoomed(); }
static inline void draw_region_rotozoomed_at(int x, int y)
{ set_drawing_point(x, y); vircon__gpu_draw_region_rotozoomed(); }

/* Function-like macros preserve the official six/four-argument call surface
 * without exceeding the current four-argument defined-call ABI. Each input is
 * first evaluated once into a block-local i32, then applied to GPU state. */
#define define_region(min_x, min_y, max_x, max_y, hotspot_x, hotspot_y) \
    do { \
        int vircon__region_min_x = (min_x); \
        int vircon__region_min_y = (min_y); \
        int vircon__region_max_x = (max_x); \
        int vircon__region_max_y = (max_y); \
        int vircon__region_hotspot_x = (hotspot_x); \
        int vircon__region_hotspot_y = (hotspot_y); \
        set_region_minimum(vircon__region_min_x, vircon__region_min_y); \
        set_region_maximum(vircon__region_max_x, vircon__region_max_y); \
        set_region_hotspot(vircon__region_hotspot_x, vircon__region_hotspot_y); \
    } while (0)

/* Defines the selected region with a top-left hotspot. Arguments are evaluated
 * once; this is a macro because a normal six-argument helper exceeds the ABI. */
#define define_region_topleft(min_x, min_y, max_x, max_y) \
    do { \
        int vircon__region_min_x = (min_x); \
        int vircon__region_min_y = (min_y); \
        int vircon__region_max_x = (max_x); \
        int vircon__region_max_y = (max_y); \
        define_region(vircon__region_min_x, vircon__region_min_y, \
                      vircon__region_max_x, vircon__region_max_y, \
                      vircon__region_min_x, vircon__region_min_y); \
    } while (0)

/* Define the selected region with its integer-centre hotspot. */
static inline void define_region_center(int min_x, int min_y, int max_x, int max_y)
{
    set_region_minimum(min_x, min_y); set_region_maximum(max_x, max_y);
    set_region_hotspot((min_x + max_x) / 2, (min_y + max_y) / 2);
}

/* A regular region matrix. Prefer static const descriptions. */
struct vircon_region_matrix {
    int first_id, first_min_x, first_min_y, first_max_x, first_max_y;
    int first_hotspot_x, first_hotspot_y, elements_x, elements_y, gap;
};
static inline void define_region_matrix(const struct vircon_region_matrix *d)
{
    int id = d->first_id, min_x = d->first_min_x, min_y = d->first_min_y;
    int max_x = d->first_max_x, max_y = d->first_max_y;
    int hotspot_x = d->first_hotspot_x, hotspot_y = d->first_hotspot_y;
    int advance_x = max_x - min_x + 1 + d->gap;
    int advance_y = max_y - min_y + 1 + d->gap;
    int matrix_y;
    for (matrix_y = 0; matrix_y < d->elements_y; ++matrix_y) {
        int matrix_x;
        for (matrix_x = 0; matrix_x < d->elements_x; ++matrix_x) {
            select_region(id++); set_region_minimum(min_x, min_y);
            set_region_maximum(max_x, max_y); set_region_hotspot(hotspot_x, hotspot_y);
            min_x += advance_x; max_x += advance_x; hotspot_x += advance_x;
        }
        min_y += advance_y; max_y += advance_y; hotspot_y += advance_y;
        min_x = d->first_min_x; max_x = d->first_max_x; hotspot_x = d->first_hotspot_x;
    }
}

/* BIOS texture -1 region 256 is a white pixel. These leave it selected. */
static inline void draw_bios_horizontal_line(int x1, int y, int x2)
{
    select_texture(-1); select_region(256); set_drawing_scale((float)(x2 - x1 + 1), 1.0f);
    set_drawing_point(x1, y); draw_region_zoomed();
}
static inline void draw_bios_vertical_line(int x, int y1, int y2)
{
    select_texture(-1); select_region(256); set_drawing_scale(1.0f, (float)(y2 - y1 + 1));
    set_drawing_point(x, y1); draw_region_zoomed();
}

/* Input, time and RNG ------------------------------------------------------
 * Select gamepad 0..3 before querying it. Selection is shared device state. */
static inline void select_gamepad(int id) { vircon__input_select_gamepad(id); }
static inline int gamepad_left(void) { return vircon__input_gamepad_left(); }
static inline int gamepad_right(void) { return vircon__input_gamepad_right(); }
static inline int gamepad_up(void) { return vircon__input_gamepad_up(); }
static inline int gamepad_down(void) { return vircon__input_gamepad_down(); }
static inline int gamepad_is_connected(void) { return vircon__input_gamepad_connected(); }
static inline int gamepad_button_a(void) { return vircon__input_gamepad_button_a(); }
static inline int gamepad_button_b(void) { return vircon__input_gamepad_button_b(); }
static inline int gamepad_button_x(void) { return vircon__input_gamepad_button_x(); }
static inline int gamepad_button_y(void) { return vircon__input_gamepad_button_y(); }
static inline int gamepad_button_l(void) { return vircon__input_gamepad_button_l(); }
static inline int gamepad_button_r(void) { return vircon__input_gamepad_button_r(); }
static inline int gamepad_button_start(void) { return vircon__input_gamepad_button_start(); }
static inline int gamepad_direction_x(void)
{ if (gamepad_left() > 0) return -1; if (gamepad_right() > 0) return 1; return 0; }
static inline int gamepad_direction_y(void)
{ if (gamepad_up() > 0) return -1; if (gamepad_down() > 0) return 1; return 0; }
static inline void gamepad_direction(int *x, int *y)
{ *x = gamepad_direction_x(); *y = gamepad_direction_y(); }
static inline int get_frame_counter(void) { return vircon__timer_get_frame_counter(); }
/* Raw Vircon time/date values; no calendar or timezone library is included. */
static inline int get_time(void) { return vircon__timer_get_current_time(); }
static inline int get_date(void) { return vircon__timer_get_current_date(); }
/* Direct wrappers over Vircon RNG state, not a hosted libc RNG. */
static inline int rand(void) { return vircon__rng_get_current_value(); }
static inline void srand(int seed) { vircon__rng_set_current_value(seed); }
static inline void sleep(int frames)
{ int final_frame = get_frame_counter() + frames; while (get_frame_counter() < final_frame) end_frame(); }

/* Sound --------------------------------------------------------------------
 * Sound IDs are --sound argument order. Vircon has channels 0..15, and all
 * channel helpers select their supplied channel before acting. */
static inline void select_sound(int id) { vircon__spu_select_sound(id); }
static inline void set_sound_loop(int enabled) { vircon__spu_set_sound_play_with_loop(enabled); }
/* Loop offsets are samples within the selected sound. */
static inline void set_sound_loop_start(int position) { vircon__spu_set_sound_loop_start(position); }
static inline void set_sound_loop_end(int position) { vircon__spu_set_sound_loop_end(position); }
static inline void select_channel(int id) { vircon__spu_select_channel(id); }
static inline void assign_channel_sound(int channel, int sound)
{ select_channel(channel); vircon__spu_set_channel_assigned_sound(sound); }
static inline void play_sound_in_channel(int sound, int channel)
{ assign_channel_sound(channel, sound); vircon__spu_play_selected_channel(); }
static inline void set_channel_volume(float value) { vircon__spu_set_channel_volume(value); }
static inline void set_channel_speed(float value) { vircon__spu_set_channel_speed(value); }
static inline void set_channel_loop(int enabled) { vircon__spu_set_channel_loop_enabled(enabled); }
static inline void set_global_volume(float value) { vircon__spu_set_global_volume(value); }
static inline void play_channel(int channel)
{ select_channel(channel); vircon__spu_play_selected_channel(); }
static inline void pause_channel(int channel)
{ select_channel(channel); vircon__spu_pause_selected_channel(); }
/* States are 0x40 stopped, 0x41 paused, and 0x42 playing. */
static inline int get_channel_state(int channel)
{ select_channel(channel); return vircon__spu_get_channel_state(); }
/* Start on the first stopped channel; return that channel or -1 if none. */
static inline int play_sound(int sound)
{
    int channel;
    for (channel = 0; channel < 16; ++channel) {
        select_channel(channel);
        if (vircon__spu_get_channel_state() == 0x40) {
            vircon__spu_set_channel_assigned_sound(sound);
            vircon__spu_play_selected_channel();
            return channel;
        }
    }
    return -1;
}

/* Memory card --------------------------------------------------------------
 * Vircon cards are word-addressed.  These public helpers deliberately use
 * `int` words too: a normal C `int *` advances four Wasm bytes while one card
 * index advances one Vircon card word.  Card offsets and counts are words,
 * not byte counts.  The raw memory-mapped device address remains private. */
static inline int card_is_connected(void) { return vircon__memcard_is_connected(); }
static inline int card_read_word(int word_index)
{ return vircon__memcard_read_word(word_index); }
static inline void card_write_word(int word_index, int value)
{ vircon__memcard_write_word(word_index, value); }
static inline void card_read_words(int *destination, int card_word_offset, int word_count)
{
    int index;
    for (index = 0; index < word_count; ++index)
        destination[index] = card_read_word(card_word_offset + index);
}
static inline void card_write_words(const int *source, int card_word_offset, int word_count)
{
    int index;
    for (index = 0; index < word_count; ++index)
        card_write_word(card_word_offset + index, source[index]);
}
static inline int card_words_match(const int *expected, int card_word_offset, int word_count)
{
    int index;
    for (index = 0; index < word_count; ++index)
        if (card_read_word(card_word_offset + index) != expected[index])
            return 0;
    return 1;
}

/* Finite f32 math ----------------------------------------------------------
 * A narrow ordinary-C math layer over Vircon CPU operations. It is not libm:
 * use finite inputs in the target instruction domains; NaN/infinity and all
 * edge cases are intentionally not promised. */
static inline float sinf(float x) { return vircon__cpu_sin(x); }
static inline float cosf(float x) { return vircon__cpu_sin(x + 1.57079632679f); }
static inline float tanf(float x) { return sinf(x) / cosf(x); }
static inline float acosf(float x) { return vircon__cpu_acos(x); }
static inline float asinf(float x) { return 1.57079632679f - acosf(x); }
static inline float expf(float x) { return vircon__cpu_pow(2.71828182846f, x); }
static inline float logf(float x) { return vircon__cpu_log(x); }
static inline float powf(float x, float y) { return vircon__cpu_pow(x, y); }

#if defined(__clang__)
#pragma clang attribute pop
#endif

#endif
