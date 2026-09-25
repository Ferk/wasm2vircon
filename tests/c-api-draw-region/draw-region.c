#include <vircon.h>

/* Covers every official region-draw call shape exposed by the public header. */
int main(void)
{
    draw_region();
    draw_region_at(10, 20);
    draw_region_zoomed();
    draw_region_zoomed_at(30, 40);
    draw_region_rotated();
    draw_region_rotated_at(50, 60);
    draw_region_rotozoomed();
    draw_region_rotozoomed_at(70, 80);
    return 0;
}
