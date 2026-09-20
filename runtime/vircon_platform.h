#ifndef VIRCON_PLATFORM_H
#define VIRCON_PLATFORM_H

/* C bindings for the language-neutral platform-import registry proposed in
 * docs/runtime-text-analysis.md. Only declarations live here. */
#if defined(__wasm__)
#define VIRCON_IMPORT(name) __attribute__((import_module("env"), import_name(name)))
#else
#define VIRCON_IMPORT(name)
#endif

void vircon_set_background_color(int color)
    VIRCON_IMPORT("vircon_set_background_color");
void vircon_end_frame(void) VIRCON_IMPORT("vircon_end_frame");

int vircon_gpu_get_selected_texture(void)
    VIRCON_IMPORT("vircon_gpu_get_selected_texture");
void vircon_gpu_select_texture(int texture_id)
    VIRCON_IMPORT("vircon_gpu_select_texture");
void vircon_gpu_select_region(int region_id)
    VIRCON_IMPORT("vircon_gpu_select_region");
void vircon_gpu_set_drawing_point(int x, int y)
    VIRCON_IMPORT("vircon_gpu_set_drawing_point");
void vircon_gpu_draw_region(void) VIRCON_IMPORT("vircon_gpu_draw_region");
void vircon_gpu_set_region_minimum(int x, int y)
    VIRCON_IMPORT("vircon_gpu_set_region_minimum");
void vircon_gpu_set_region_maximum(int x, int y)
    VIRCON_IMPORT("vircon_gpu_set_region_maximum");
void vircon_gpu_set_region_hotspot(int x, int y)
    VIRCON_IMPORT("vircon_gpu_set_region_hotspot");
void vircon_gpu_set_multiply_color(int color)
    VIRCON_IMPORT("vircon_gpu_set_multiply_color");
void vircon_gpu_set_drawing_scale_bits(int scale_x_bits, int scale_y_bits)
    VIRCON_IMPORT("vircon_gpu_set_drawing_scale_bits");
void vircon_gpu_set_drawing_scale(float scale_x, float scale_y)
    VIRCON_IMPORT("vircon_gpu_set_drawing_scale");
void vircon_gpu_draw_region_zoomed(void)
    VIRCON_IMPORT("vircon_gpu_draw_region_zoomed");

float vircon_cpu_sin(float value) VIRCON_IMPORT("vircon_cpu_sin");
float vircon_cpu_acos(float value) VIRCON_IMPORT("vircon_cpu_acos");
float vircon_cpu_log(float value) VIRCON_IMPORT("vircon_cpu_log");
float vircon_cpu_pow(float base, float exponent)
    VIRCON_IMPORT("vircon_cpu_pow");

void vircon_input_select_gamepad(int gamepad_id)
    VIRCON_IMPORT("vircon_input_select_gamepad");
int vircon_input_gamepad_left(void) VIRCON_IMPORT("vircon_input_gamepad_left");
int vircon_input_gamepad_right(void) VIRCON_IMPORT("vircon_input_gamepad_right");
int vircon_input_gamepad_up(void) VIRCON_IMPORT("vircon_input_gamepad_up");
int vircon_input_gamepad_down(void) VIRCON_IMPORT("vircon_input_gamepad_down");
int vircon_input_gamepad_connected(void)
    VIRCON_IMPORT("vircon_input_gamepad_connected");
int vircon_input_gamepad_button_a(void)
    VIRCON_IMPORT("vircon_input_gamepad_button_a");
int vircon_input_gamepad_button_b(void)
    VIRCON_IMPORT("vircon_input_gamepad_button_b");
int vircon_input_gamepad_button_x(void)
    VIRCON_IMPORT("vircon_input_gamepad_button_x");
int vircon_input_gamepad_button_y(void)
    VIRCON_IMPORT("vircon_input_gamepad_button_y");
int vircon_input_gamepad_button_l(void)
    VIRCON_IMPORT("vircon_input_gamepad_button_l");
int vircon_input_gamepad_button_r(void)
    VIRCON_IMPORT("vircon_input_gamepad_button_r");
int vircon_input_gamepad_button_start(void)
    VIRCON_IMPORT("vircon_input_gamepad_button_start");
int vircon_timer_get_frame_counter(void)
    VIRCON_IMPORT("vircon_timer_get_frame_counter");
int vircon_timer_get_current_time(void)
    VIRCON_IMPORT("vircon_timer_get_current_time");
int vircon_timer_get_current_date(void)
    VIRCON_IMPORT("vircon_timer_get_current_date");

int vircon_rng_get_current_value(void)
    VIRCON_IMPORT("vircon_rng_get_current_value");
void vircon_rng_set_current_value(int seed)
    VIRCON_IMPORT("vircon_rng_set_current_value");

void vircon_spu_select_channel(int channel_id)
    VIRCON_IMPORT("vircon_spu_select_channel");
void vircon_spu_set_channel_assigned_sound(int sound_id)
    VIRCON_IMPORT("vircon_spu_set_channel_assigned_sound");
void vircon_spu_play_selected_channel(void)
    VIRCON_IMPORT("vircon_spu_play_selected_channel");
void vircon_spu_set_channel_volume(float volume)
    VIRCON_IMPORT("vircon_spu_set_channel_volume");
void vircon_spu_set_channel_speed(float speed)
    VIRCON_IMPORT("vircon_spu_set_channel_speed");
int vircon_spu_get_channel_state(void)
    VIRCON_IMPORT("vircon_spu_get_channel_state");

#endif
