#include "vircon.h"

/* The BIOS texture (-1), region 256 is its single white pixel. These helpers
 * reproduce the official draw_primitives.h line behaviour using ordinary C. */
void draw_bios_horizontal_line(int x1, int y1, int x2)
{
    select_texture(-1);
    select_region(256);
    set_drawing_scale((float)(x2 - x1 + 1), 1.0f);
    set_drawing_point(x1, y1);
    draw_region_zoomed();
}

void draw_bios_vertical_line(int x1, int y1, int y2)
{
    select_texture(-1);
    select_region(256);
    set_drawing_scale(1.0f, (float)(y2 - y1 + 1));
    set_drawing_point(x1, y1);
    draw_region_zoomed();
}
