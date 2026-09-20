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

void vircon_input_select_gamepad(int gamepad_id)
    VIRCON_IMPORT("vircon_input_select_gamepad");
int vircon_input_gamepad_left(void) VIRCON_IMPORT("vircon_input_gamepad_left");
int vircon_input_gamepad_right(void) VIRCON_IMPORT("vircon_input_gamepad_right");
int vircon_input_gamepad_up(void) VIRCON_IMPORT("vircon_input_gamepad_up");
int vircon_input_gamepad_down(void) VIRCON_IMPORT("vircon_input_gamepad_down");
int vircon_timer_get_frame_counter(void)
    VIRCON_IMPORT("vircon_timer_get_frame_counter");

void vircon_spu_select_channel(int channel_id)
    VIRCON_IMPORT("vircon_spu_select_channel");
void vircon_spu_set_channel_assigned_sound(int sound_id)
    VIRCON_IMPORT("vircon_spu_set_channel_assigned_sound");
void vircon_spu_play_selected_channel(void)
    VIRCON_IMPORT("vircon_spu_play_selected_channel");

#endif
