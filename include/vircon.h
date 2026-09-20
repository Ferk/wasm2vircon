#ifndef VIRCON_RUNTIME_H
#define VIRCON_RUNTIME_H

/* Application-facing runtime API. print_at is library code, not a Wasm import.
 * Its text argument is a NUL-terminated CP-1252 byte string, not UTF-8. */
void clear_screen(int color);
void print_at(int drawing_x, int drawing_y, const char *text);
void end_frame(void);

/* Basic cartridge-texture operations. These are runtime functions, not Wasm
 * compiler intrinsics. */
void select_texture(int texture_id);
void select_region(int region_id);
void set_region_minimum(int x, int y);
void set_region_maximum(int x, int y);
void set_region_hotspot(int x, int y);
void draw_region_at(int drawing_x, int drawing_y);
void set_multiply_color(int color);

/* TileMap's small input/timer surface. Gamepad direction writes -1, 0, or 1
 * through normal byte-addressed C pointers. */
void select_gamepad(int gamepad_id);
void gamepad_direction(int *delta_x, int *delta_y);
int gamepad_direction_x(void);
int gamepad_direction_y(void);
int get_frame_counter(void);

/* A runtime representation avoids a general ten-argument Wasm call while
 * retaining the official define_region_matrix behaviour. */
struct vircon_region_matrix {
    int first_id;
    int first_min_x;
    int first_min_y;
    int first_max_x;
    int first_max_y;
    int first_hotspot_x;
    int first_hotspot_y;
    int elements_x;
    int elements_y;
    int gap;
};
void define_region_matrix(const struct vircon_region_matrix *definition);

/* Kept inline so a constant region description remains ordinary application
 * C and needs no six-argument backend ABI. */
static inline void define_region_center(int min_x, int min_y,
                                        int max_x, int max_y)
{
    set_region_minimum(min_x, min_y);
    set_region_maximum(max_x, max_y);
    set_region_hotspot((min_x + max_x) / 2, (min_y + max_y) / 2);
}

/* Minimal selected-channel sound helper. */
void play_sound_in_channel(int sound_id, int channel_id);

#endif
