#include "vircon.h"
#include "vircon_platform.h"

#define VIRCON_BIOS_GLYPH_WIDTH 10
#define VIRCON_BIOS_GLYPH_HEIGHT 20

/* Mirrors the official helper's cursor and state-preservation behavior, but
 * reads byte-addressed Wasm strings rather than the official C toolchain's
 * word-addressed int* strings. Nonzero CP-1252 bytes map directly to BIOS
 * glyph-region IDs; this is deliberately not a UTF-8 decoder. */
void print_at(int drawing_x, int drawing_y, const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;
    const int initial_drawing_x = drawing_x;
    const int previous_texture = vircon_gpu_get_selected_texture();

    vircon_gpu_select_texture(-1);
    while (*cursor != 0) {
        const int glyph = *cursor;

        vircon_gpu_select_region(glyph);
        vircon_gpu_set_drawing_point(drawing_x, drawing_y);
        vircon_gpu_draw_region();

        drawing_x += VIRCON_BIOS_GLYPH_WIDTH;
        if (glyph == '\n') {
            drawing_x = initial_drawing_x;
            drawing_y += VIRCON_BIOS_GLYPH_HEIGHT;
        }
        ++cursor;
    }
    vircon_gpu_select_texture(previous_texture);
}
