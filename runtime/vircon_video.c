#include "vircon.h"
#include "vircon_platform.h"

void select_texture(int texture_id)
{
    vircon_gpu_select_texture(texture_id);
}

void select_region(int region_id)
{
    vircon_gpu_select_region(region_id);
}

void set_region_minimum(int x, int y)
{
    vircon_gpu_set_region_minimum(x, y);
}

void set_region_maximum(int x, int y)
{
    vircon_gpu_set_region_maximum(x, y);
}

void set_region_hotspot(int x, int y)
{
    vircon_gpu_set_region_hotspot(x, y);
}

void draw_region_at(int drawing_x, int drawing_y)
{
    vircon_gpu_set_drawing_point(drawing_x, drawing_y);
    vircon_gpu_draw_region();
}
