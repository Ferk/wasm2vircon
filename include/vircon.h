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
 * This is not a hosted libc.  It has no stdio, printf, general allocation,
 * files, locale, or UTF-8 support.  It supplies the documented small byte
 * string/memory helpers. Use only public names;
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
int vircon__gpu_get_multiply_color(void) VIRCON__IMPORT("vircon_gpu_get_multiply_color");
void vircon__gpu_set_active_blending(int value) VIRCON__IMPORT("vircon_gpu_set_active_blending");
int vircon__gpu_get_active_blending(void) VIRCON__IMPORT("vircon_gpu_get_active_blending");
int vircon__gpu_get_drawing_point_x(void) VIRCON__IMPORT("vircon_gpu_get_drawing_point_x");
int vircon__gpu_get_drawing_point_y(void) VIRCON__IMPORT("vircon_gpu_get_drawing_point_y");
void vircon__gpu_set_drawing_scale_bits(int x, int y) VIRCON__IMPORT("vircon_gpu_set_drawing_scale_bits");
void vircon__gpu_set_drawing_scale(float x, float y) VIRCON__IMPORT("vircon_gpu_set_drawing_scale");
float vircon__gpu_get_drawing_scale_x(void) VIRCON__IMPORT("vircon_gpu_get_drawing_scale_x");
float vircon__gpu_get_drawing_scale_y(void) VIRCON__IMPORT("vircon_gpu_get_drawing_scale_y");
void vircon__gpu_set_drawing_angle(float value) VIRCON__IMPORT("vircon_gpu_set_drawing_angle");
float vircon__gpu_get_drawing_angle(void) VIRCON__IMPORT("vircon_gpu_get_drawing_angle");
void vircon__gpu_draw_region_zoomed(void) VIRCON__IMPORT("vircon_gpu_draw_region_zoomed");
void vircon__gpu_draw_region_rotated(void) VIRCON__IMPORT("vircon_gpu_draw_region_rotated");
void vircon__gpu_draw_region_rotozoomed(void) VIRCON__IMPORT("vircon_gpu_draw_region_rotozoomed");
float vircon__cpu_sin(float value) VIRCON__IMPORT("vircon_cpu_sin");
float vircon__cpu_acos(float value) VIRCON__IMPORT("vircon_cpu_acos");
float vircon__cpu_log(float value) VIRCON__IMPORT("vircon_cpu_log");
float vircon__cpu_pow(float x, float y) VIRCON__IMPORT("vircon_cpu_pow");
float vircon__cpu_fmod(float x, float y) VIRCON__IMPORT("vircon_cpu_fmod");
int vircon__cpu_imin(int x, int y) VIRCON__IMPORT("vircon_cpu_imin");
int vircon__cpu_imax(int x, int y) VIRCON__IMPORT("vircon_cpu_imax");
int vircon__cpu_iabs(int value) VIRCON__IMPORT("vircon_cpu_iabs");
float vircon__cpu_fmin(float x, float y) VIRCON__IMPORT("vircon_cpu_fmin");
float vircon__cpu_fmax(float x, float y) VIRCON__IMPORT("vircon_cpu_fmax");
float vircon__cpu_fabs(float value) VIRCON__IMPORT("vircon_cpu_fabs");
float vircon__cpu_floor(float value) VIRCON__IMPORT("vircon_cpu_floor");
float vircon__cpu_ceil(float value) VIRCON__IMPORT("vircon_cpu_ceil");
float vircon__cpu_round(float value) VIRCON__IMPORT("vircon_cpu_round");
float vircon__cpu_atan2(float y, float x) VIRCON__IMPORT("vircon_cpu_atan2");
void vircon__cpu_halt(void) VIRCON__IMPORT("vircon_cpu_halt");
void vircon__input_select_gamepad(int value) VIRCON__IMPORT("vircon_input_select_gamepad");
int vircon__input_get_selected_gamepad(void) VIRCON__IMPORT("vircon_input_get_selected_gamepad");
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
int vircon__timer_get_cycle_counter(void) VIRCON__IMPORT("vircon_timer_get_cycle_counter");
int vircon__timer_get_current_time(void) VIRCON__IMPORT("vircon_timer_get_current_time");
int vircon__timer_get_current_date(void) VIRCON__IMPORT("vircon_timer_get_current_date");
int vircon__rng_get_current_value(void) VIRCON__IMPORT("vircon_rng_get_current_value");
void vircon__rng_set_current_value(int value) VIRCON__IMPORT("vircon_rng_set_current_value");
int vircon__memcard_is_connected(void) VIRCON__IMPORT("vircon_memcard_is_connected");
int vircon__memcard_read_word(int word_index) VIRCON__IMPORT("vircon_memcard_read_word");
void vircon__memcard_write_word(int word_index, int value) VIRCON__IMPORT("vircon_memcard_write_word");
void vircon__spu_select_channel(int value) VIRCON__IMPORT("vircon_spu_select_channel");
void vircon__spu_select_sound(int value) VIRCON__IMPORT("vircon_spu_select_sound");
int vircon__spu_get_selected_sound(void) VIRCON__IMPORT("vircon_spu_get_selected_sound");
int vircon__spu_get_selected_channel(void) VIRCON__IMPORT("vircon_spu_get_selected_channel");
void vircon__spu_set_sound_play_with_loop(int value) VIRCON__IMPORT("vircon_spu_set_sound_play_with_loop");
void vircon__spu_set_sound_loop_start(int value) VIRCON__IMPORT("vircon_spu_set_sound_loop_start");
void vircon__spu_set_sound_loop_end(int value) VIRCON__IMPORT("vircon_spu_set_sound_loop_end");
void vircon__spu_set_channel_assigned_sound(int value) VIRCON__IMPORT("vircon_spu_set_channel_assigned_sound");
void vircon__spu_play_selected_channel(void) VIRCON__IMPORT("vircon_spu_play_selected_channel");
void vircon__spu_pause_selected_channel(void) VIRCON__IMPORT("vircon_spu_pause_selected_channel");
void vircon__spu_stop_selected_channel(void) VIRCON__IMPORT("vircon_spu_stop_selected_channel");
void vircon__spu_set_channel_volume(float value) VIRCON__IMPORT("vircon_spu_set_channel_volume");
void vircon__spu_set_channel_speed(float value) VIRCON__IMPORT("vircon_spu_set_channel_speed");
void vircon__spu_set_channel_position(int value) VIRCON__IMPORT("vircon_spu_set_channel_position");
void vircon__spu_set_channel_loop_enabled(int value) VIRCON__IMPORT("vircon_spu_set_channel_loop_enabled");
void vircon__spu_set_global_volume(float value) VIRCON__IMPORT("vircon_spu_set_global_volume");
int vircon__spu_get_channel_state(void) VIRCON__IMPORT("vircon_spu_get_channel_state");
float vircon__spu_get_channel_speed(void) VIRCON__IMPORT("vircon_spu_get_channel_speed");
int vircon__spu_get_channel_position(void) VIRCON__IMPORT("vircon_spu_get_channel_position");
float vircon__spu_get_global_volume(void) VIRCON__IMPORT("vircon_spu_get_global_volume");
void vircon__spu_pause_all_channels(void) VIRCON__IMPORT("vircon_spu_pause_all_channels");
void vircon__spu_stop_all_channels(void) VIRCON__IMPORT("vircon_spu_stop_all_channels");
void vircon__spu_resume_all_channels(void) VIRCON__IMPORT("vircon_spu_resume_all_channels");

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

/* Video constants and packed Vircon ABGR colour helpers -------------------
 * Colour function components are expected in 0..255 and intentionally are
 * not clamped, matching the official header's direct bit packing behavior. */
#define screen_width 640
#define screen_height 360

#define color_black     0xFF000000
#define color_white     0xFFFFFFFF
#define color_gray      0xFF808080
#define color_darkgray  0xFF404040
#define color_lightgray 0xFFC0C0C0
#define color_red       0xFF0000FF
#define color_green     0xFF00FF00
#define color_blue      0xFFFF0000
#define color_yellow    0xFF00FFFF
#define color_magenta   0xFFFF00FF
#define color_cyan      0xFFFFFF00
#define color_orange    0xFF0080FF
#define color_brown     0xFF204080

/* Packs an opaque grey RGB value in Vircon's written ABGR word order. */
static inline int make_gray(int brightness)
{ return 0xFF000000 | (brightness << 16) | (brightness << 8) | brightness; }

/* Packs opaque RGB components in Vircon's written ABGR word order. */
static inline int make_color_rgb(int red, int green, int blue)
{ return 0xFF000000 | (blue << 16) | (green << 8) | red; }

/* Packs RGBA components in Vircon's written ABGR word order. */
static inline int make_color_rgba(int red, int green, int blue, int alpha)
{ return (alpha << 24) | (blue << 16) | (green << 8) | red; }

/* Extracts one RGBA component from a packed Vircon colour word. */
static inline int get_color_red(int color) { return color & 255; }
static inline int get_color_green(int color) { return (color >> 8) & 255; }
static inline int get_color_blue(int color) { return (color >> 16) & 255; }
static inline int get_color_alpha(int color) { return (color >> 24) & 255; }

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
static inline int get_multiply_color(void) { return vircon__gpu_get_multiply_color(); }
/* Tested blend mode words: alpha 0x20, add 0x21, subtract 0x22. */
static inline void set_blending_mode(int mode) { vircon__gpu_set_active_blending(mode); }
static inline int get_blending_mode(void) { return vircon__gpu_get_active_blending(); }
/* Stores the current point in normal byte-addressed C int objects. */
static inline void get_drawing_point(int *x, int *y)
{ *x = vircon__gpu_get_drawing_point_x(); *y = vircon__gpu_get_drawing_point_y(); }
/* Use this typed f32 form for calculated scale values. */
static inline void set_drawing_scale(float x, float y) { vircon__gpu_set_drawing_scale(x, y); }
/* Stores the current scale in normal byte-addressed C float objects. */
static inline void get_drawing_scale(float *x, float *y)
{ *x = vircon__gpu_get_drawing_scale_x(); *y = vircon__gpu_get_drawing_scale_y(); }
/* For already-known IEEE-754 f32 bits only; calculated values use the typed form. */
static inline void set_drawing_scale_bits(int x_bits, int y_bits)
{ vircon__gpu_set_drawing_scale_bits(x_bits, y_bits); }
static inline void set_drawing_angle(float angle) { vircon__gpu_set_drawing_angle(angle); }
static inline float get_drawing_angle(void) { return vircon__gpu_get_drawing_angle(); }
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
#define frames_per_second 60
#define frame_time (1.0f / frames_per_second)

/* Normal-C forms of the official human-readable timer structures. */
typedef struct date_info {
    int year;
    int month;
    int day;
} date_info;

typedef struct time_info {
    int hours;
    int minutes;
    int seconds;
} time_info;

static inline void select_gamepad(int id) { vircon__input_select_gamepad(id); }
/* Returns the input device's currently selected gamepad. */
static inline int get_selected_gamepad(void) { return vircon__input_get_selected_gamepad(); }
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
/* Converts D-pad input to a unit vector, normalizing diagonal movement. */
static inline void gamepad_direction_normalized(float *x, float *y)
{
    int direction_x = gamepad_direction_x();
    int direction_y = gamepad_direction_y();
    *x = (float)direction_x;
    *y = (float)direction_y;
    if (direction_x != 0 && direction_y != 0) {
        *x *= 0.70710678f;
        *y *= 0.70710678f;
    }
}

/* Reads CPU cycles elapsed in the current frame; emulators need not be exact. */
static inline int get_cycle_counter(void) { return vircon__timer_get_cycle_counter(); }
static inline int get_frame_counter(void) { return vircon__timer_get_frame_counter(); }
/* Raw Vircon time/date values; no calendar or timezone library is included. */
static inline int get_time(void) { return vircon__timer_get_current_time(); }
static inline int get_date(void) { return vircon__timer_get_current_date(); }
/* Splits elapsed seconds since midnight into ordinary clock fields. */
static inline void translate_time(int time, time_info *translated)
{
    /* The hardware's current-time value is always a nonnegative count of
     * seconds since midnight.  Unsigned intermediates also avoid treating
     * the packed conversion as a signed hosted-time calculation. */
    unsigned value = (unsigned)time;
    translated->hours = (int)(value / 3600u);
    translated->minutes = (int)((value % 3600u) / 60u);
    translated->seconds = (int)(value % 60u);
}
/* Splits Vircon's packed year/day-of-year value into calendar fields. */
static inline void translate_date(int date, date_info *translated)
{
    static const int month_days[12] = {31, 28, 31, 30, 31, 30,
                                       31, 31, 30, 31, 30, 31};
    int days_in_year = date & 0x0000FFFF;
    int month;
    int leap_year;

    translated->year = date >> 16;
    leap_year = ((translated->year % 4) == 0) && ((translated->year % 100) != 0);
    for (month = 0; month < 11; ++month) {
        int days_in_month = month_days[month];
        if (month == 1 && leap_year)
            days_in_month = 29;
        if (days_in_year < days_in_month) {
            translated->month = month + 1;
            translated->day = days_in_year + 1;
            return;
        }
        days_in_year -= days_in_month;
    }
    translated->month = 12;
    translated->day = days_in_year + 1;
}
/* Direct wrappers over Vircon RNG state, not a hosted libc RNG. */
static inline int rand(void) { return vircon__rng_get_current_value(); }
static inline void srand(int seed) { vircon__rng_set_current_value(seed); }
static inline void sleep(int frames)
{ int final_frame = get_frame_counter() + frames; while (get_frame_counter() < final_frame) end_frame(); }

/* Sound --------------------------------------------------------------------
 * Sound IDs are --sound argument order. Vircon has channels 0..15, and all
 * channel helpers select their supplied channel before acting. */
#define sound_channels 16
#define channel_stopped 0x40
#define channel_paused  0x41
#define channel_playing 0x42

static inline void select_sound(int id) { vircon__spu_select_sound(id); }
/* Returns the sound resource selected in the SPU. */
static inline int get_selected_sound(void) { return vircon__spu_get_selected_sound(); }
static inline void set_sound_loop(int enabled) { vircon__spu_set_sound_play_with_loop(enabled); }
/* Loop offsets are samples within the selected sound. */
static inline void set_sound_loop_start(int position) { vircon__spu_set_sound_loop_start(position); }
static inline void set_sound_loop_end(int position) { vircon__spu_set_sound_loop_end(position); }
static inline void select_channel(int id) { vircon__spu_select_channel(id); }
/* Returns the channel currently selected in the SPU. */
static inline int get_selected_channel(void) { return vircon__spu_get_selected_channel(); }
static inline void assign_channel_sound(int channel, int sound)
{ select_channel(channel); vircon__spu_set_channel_assigned_sound(sound); }
static inline void play_sound_in_channel(int sound, int channel)
{ assign_channel_sound(channel, sound); vircon__spu_play_selected_channel(); }
static inline void set_channel_volume(float value) { vircon__spu_set_channel_volume(value); }
static inline void set_channel_speed(float value) { vircon__spu_set_channel_speed(value); }
/* Sets the selected channel position in samples from its sound start. */
static inline void set_channel_position(int position) { vircon__spu_set_channel_position(position); }
static inline void set_channel_loop(int enabled) { vircon__spu_set_channel_loop_enabled(enabled); }
static inline void set_global_volume(float value) { vircon__spu_set_global_volume(value); }
static inline void play_channel(int channel)
{ select_channel(channel); vircon__spu_play_selected_channel(); }
static inline void pause_channel(int channel)
{ select_channel(channel); vircon__spu_pause_selected_channel(); }
/* Selects and stops one channel. */
static inline void stop_channel(int channel)
{ select_channel(channel); vircon__spu_stop_selected_channel(); }
/* States are channel_stopped, channel_paused, and channel_playing. */
static inline int get_channel_state(int channel)
{ select_channel(channel); return vircon__spu_get_channel_state(); }
/* These queries select their channel, matching the official Vircon helpers. */
static inline float get_channel_speed(int channel)
{ select_channel(channel); return vircon__spu_get_channel_speed(); }
static inline int get_channel_position(int channel)
{ select_channel(channel); return vircon__spu_get_channel_position(); }
/* Returns the SPU's global volume multiplier. */
static inline float get_global_volume(void) { return vircon__spu_get_global_volume(); }
/* Issue the corresponding command to every SPU channel. */
static inline void pause_all_channels(void) { vircon__spu_pause_all_channels(); }
static inline void stop_all_channels(void) { vircon__spu_stop_all_channels(); }
static inline void resume_all_channels(void) { vircon__spu_resume_all_channels(); }
/* Start on the first stopped channel; return that channel or -1 if none. */
static inline int play_sound(int sound)
{
    int channel;
    for (channel = 0; channel < 16; ++channel) {
        select_channel(channel);
        if (vircon__spu_get_channel_state() == channel_stopped) {
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
#define game_signature_words 20
typedef int game_signature[game_signature_words];

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
/* Reads/writes the 20-word standard card signature. */
static inline void card_read_signature(game_signature *signature)
{ card_read_words(*signature, 0, game_signature_words); }
static inline void card_write_signature(const game_signature *signature)
{ card_write_words(*signature, 0, game_signature_words); }
static inline int card_signature_matches(const game_signature *expected_signature)
{ return card_words_match(*expected_signature, 0, game_signature_words); }
/* A card with an all-zero standard signature is considered empty. */
static inline int card_is_empty(void)
{
    static const game_signature zero_signature = {0};
    return card_signature_matches(&zero_signature);
}
/* Copies card words to a byte-addressed C buffer in little-endian order.
 * card_word_offset and word_count are card words, so the buffer needs at
 * least word_count * 4 bytes. */
static inline void card_read_data(void *destination, int card_word_offset, int word_count)
{
    unsigned char *bytes = (unsigned char *)destination;
    int index;
    for (index = 0; index < word_count; ++index) {
        unsigned word = (unsigned)card_read_word(card_word_offset + index);
        bytes[index * 4] = (unsigned char)word;
        bytes[index * 4 + 1] = (unsigned char)(word >> 8);
        bytes[index * 4 + 2] = (unsigned char)(word >> 16);
        bytes[index * 4 + 3] = (unsigned char)(word >> 24);
    }
}
/* Packs a byte-addressed C buffer as little-endian card words before writing.
 * card_word_offset and word_count are card words, not byte quantities. */
static inline void card_write_data(const void *source, int card_word_offset, int word_count)
{
    const unsigned char *bytes = (const unsigned char *)source;
    int index;
    for (index = 0; index < word_count; ++index) {
        unsigned byte_index = (unsigned)index * 4u;
        unsigned word = (unsigned)bytes[byte_index]
            | ((unsigned)bytes[byte_index + 1] << 8)
            | ((unsigned)bytes[byte_index + 2] << 16)
            | ((unsigned)bytes[byte_index + 3] << 24);
        card_write_word(card_word_offset + index, (int)word);
    }
}

/* Freestanding byte/string helpers ----------------------------------------
 * These use normal C byte pointers and do not share the official compiler's
 * word-character string representation.  Callers remain responsible for
 * valid buffers and capacities, as in the corresponding C routines. */
static inline int isdigit(int character)
{ return character >= '0' && character <= '9'; }
static inline int isxdigit(int character)
{
    return isdigit(character)
        || (character >= 'a' && character <= 'f')
        || (character >= 'A' && character <= 'F');
}
static inline int isalpha(int character)
{ return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z'); }
static inline int isascii(int character)
{ return character >= 0 && character <= 127; }
static inline int isalphanum(int character)
{ return isdigit(character) || isalpha(character); }
/* Includes the Windows-1252 accented Latin ranges used by the BIOS font. */
static inline int islower(int character)
{
    return (character >= 'a' && character <= 'z')
        || (character >= 224 && character <= 254 && character != 247);
}
static inline int isupper(int character)
{
    return (character >= 'A' && character <= 'Z')
        || (character >= 192 && character <= 222 && character != 215);
}
static inline int isspace(int character)
{ return character == ' ' || character == '\n' || character == '\r' || character == '\t'; }
static inline int tolower(int character)
{ return isupper(character) ? character + 32 : character; }
static inline int toupper(int character)
{ return islower(character) ? character - 32 : character; }

/* Writes count copies of the low byte of value and returns destination. */
static inline void *memset(void *destination, int value, unsigned count)
{
    unsigned char *output = (unsigned char *)destination;
    while (count != 0) {
        *output++ = (unsigned char)value;
        --count;
    }
    return destination;
}
/* Copies non-overlapping byte regions and returns destination. */
static inline void *memcpy(void *destination, const void *source, unsigned count)
{
    unsigned char *output = (unsigned char *)destination;
    const unsigned char *input = (const unsigned char *)source;
    while (count != 0) {
        *output++ = *input++;
        --count;
    }
    return destination;
}
/* Compares byte regions as unsigned bytes, like the standard C routine. */
static inline int memcmp(const void *first, const void *second, unsigned count)
{
    const unsigned char *left = (const unsigned char *)first;
    const unsigned char *right = (const unsigned char *)second;
    while (count != 0) {
        unsigned left_byte = *left++;
        unsigned right_byte = *right++;
        if (left_byte != right_byte)
            return (int)left_byte - (int)right_byte;
        --count;
    }
    return 0;
}
/* Returns the count of bytes before a string's NUL terminator. */
static inline unsigned strlen(const char *text)
{
    unsigned length = 0;
    while (text[length] != 0)
        ++length;
    return length;
}
/* Compares NUL-terminated strings as unsigned CP-1252-compatible bytes. */
static inline int strcmp(const char *first, const char *second)
{
    while (*(const unsigned char *)first == *(const unsigned char *)second) {
        if (*first == 0)
            return 0;
        ++first;
        ++second;
    }
    return (int)*(const unsigned char *)first - (int)*(const unsigned char *)second;
}
/* Compares at most count string bytes as unsigned bytes. */
static inline int strncmp(const char *first, const char *second, unsigned count)
{
    while (count != 0 && *(const unsigned char *)first == *(const unsigned char *)second) {
        if (*first == 0)
            return 0;
        ++first;
        ++second;
        --count;
    }
    if (count == 0)
        return 0;
    return (int)*(const unsigned char *)first - (int)*(const unsigned char *)second;
}
/* Copies a NUL-terminated string and returns destination. */
static inline char *strcpy(char *destination, const char *source)
{
    char *result = destination;
    while (*source != 0)
        *destination++ = *source++;
    *destination = 0;
    return result;
}
/* Copies at most count bytes, padding remaining destination bytes with NUL. */
static inline char *strncpy(char *destination, const char *source, unsigned count)
{
    char *result = destination;
    while (count != 0 && *source != 0) {
        *destination++ = *source++;
        --count;
    }
    while (count != 0) {
        /* Volatile preserves standard byte-wise padding and prevents Clang
         * from replacing several byte stores with an unsupported i64.store. */
        *(volatile char *)destination++ = 0;
        --count;
    }
    return result;
}
/* Appends a NUL-terminated source string and returns destination. */
static inline char *strcat(char *destination, const char *source)
{
    char *result = destination;
    while (*destination != 0)
        ++destination;
    (void)strcpy(destination, source);
    return result;
}
/* Appends at most count source bytes and always writes a terminator. */
static inline char *strncat(char *destination, const char *source, unsigned count)
{
    char *result = destination;
    volatile char *output;
    while (*destination != 0)
        ++destination;
    output = (volatile char *)destination;
    while (count != 0 && *source != 0) {
        *output++ = *source++;
        --count;
    }
    *output = 0;
    return result;
}

/* Stores a positive unsigned value in the requested base, in reverse first. */
static inline void vircon__utoa(unsigned value, char *result, unsigned base)
{
    static const char digits[] = "0123456789ABCDEF";
    char *first = result;
    char *last;
    do {
        *result++ = digits[value % base];
        value /= base;
    } while (value != 0);
    *result = 0;
    last = result - 1;
    while (first < last) {
        char temporary = *first;
        *first++ = *last;
        *last-- = temporary;
    }
}
/* Converts i32 values to a NUL-terminated base-2..16 byte string.
 * Base 10 is signed; all other bases format the i32 bit pattern unsigned. */
static inline void itoa(int value, char *result, int base)
{
    unsigned magnitude;
    if (base < 2 || base > 16)
        return;
    if (base == 10 && value < 0) {
        *result++ = '-';
        magnitude = (unsigned)(-(value + 1)) + 1u;
    }
    else
        magnitude = (unsigned)value;
    vircon__utoa(magnitude, result, (unsigned)base);
}
/* Formats a finite practical-range f32 with up to five fractional digits.
 * It is intentionally a small freestanding formatter, not printf or libc. */
static inline void ftoa(float value, char *result)
{
    char *fraction_start;
    unsigned integer_part;
    unsigned fraction_part;
    unsigned scale;
    if (value < 0.0f) {
        *result++ = '-';
        value = 0.0f - value;
    }
    integer_part = (unsigned)(int)value;
    value = (value - (float)(int)integer_part) * 100000.0f + 0.5f;
    fraction_part = (unsigned)(int)value;
    if (fraction_part >= 100000u) {
        ++integer_part;
        fraction_part -= 100000u;
    }
    itoa((int)integer_part, result, 10);
    if (fraction_part == 0)
        return;
    fraction_start = result;
    while (*fraction_start != 0)
        ++fraction_start;
    *fraction_start++ = '.';
    scale = 10000u;
    while (scale > fraction_part) {
        *fraction_start++ = '0';
        scale /= 10u;
    }
    while (fraction_part % 10u == 0)
        fraction_part /= 10u;
    vircon__utoa(fraction_part, fraction_start, 10u);
}

/* Stops the Vircon CPU. There are no hosted exit-status semantics. */
static inline void exit(void) { vircon__cpu_halt(); }

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

/* Official Vircon finite-math compatibility functions. These retain the
 * target CPU's finite-domain and hardware-error behavior. */
static inline float fmod(float x, float y) { return vircon__cpu_fmod(x, y); }
static inline int min(int x, int y) { return vircon__cpu_imin(x, y); }
static inline int max(int x, int y) { return vircon__cpu_imax(x, y); }
static inline int abs(int value) { return vircon__cpu_iabs(value); }
static inline float fmin(float x, float y) { return vircon__cpu_fmin(x, y); }
static inline float fmax(float x, float y) { return vircon__cpu_fmax(x, y); }
static inline float fabs(float value) { return vircon__cpu_fabs(value); }
static inline float floor(float value) { return vircon__cpu_floor(value); }
static inline float ceil(float value) { return vircon__cpu_ceil(value); }
static inline float round(float value) { return vircon__cpu_round(value); }
static inline float asin(float value) { return asinf(value); }
static inline float atan2(float y, float x) { return vircon__cpu_atan2(y, x); }
static inline float sqrt(float value) { return powf(value, 0.5f); }

/* The unsuffixed names match the official Vircon C header. */
static inline float sin(float value) { return sinf(value); }
static inline float cos(float value) { return cosf(value); }
static inline float tan(float value) { return tanf(value); }
static inline float acos(float value) { return acosf(value); }
static inline float exp(float value) { return expf(value); }
static inline float log(float value) { return logf(value); }
static inline float pow(float x, float y) { return powf(x, y); }

#if defined(__clang__)
#pragma clang attribute pop
#endif

#endif
