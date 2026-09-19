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

#endif
