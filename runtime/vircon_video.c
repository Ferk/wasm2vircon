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

void set_multiply_color(int color)
{
    vircon_gpu_set_multiply_color(color);
}

void set_blending_mode(int mode)
{
    vircon_gpu_set_active_blending(mode);
}

void set_drawing_point(int drawing_x, int drawing_y)
{
    vircon_gpu_set_drawing_point(drawing_x, drawing_y);
}

void set_drawing_scale_bits(int scale_x_bits, int scale_y_bits)
{
    vircon_gpu_set_drawing_scale_bits(scale_x_bits, scale_y_bits);
}

void set_drawing_scale(float scale_x, float scale_y)
{
    vircon_gpu_set_drawing_scale(scale_x, scale_y);
}

void set_drawing_angle(float angle)
{
    vircon_gpu_set_drawing_angle(angle);
}

void draw_region_zoomed(void)
{
    vircon_gpu_draw_region_zoomed();
}

void draw_region_rotozoomed_at(int drawing_x, int drawing_y)
{
    set_drawing_point(drawing_x, drawing_y);
    vircon_gpu_draw_region_rotozoomed();
}

void define_region_matrix(const struct vircon_region_matrix *definition)
{
    int current_id = definition->first_id;
    int min_x = definition->first_min_x;
    int min_y = definition->first_min_y;
    int max_x = definition->first_max_x;
    int max_y = definition->first_max_y;
    int hotspot_x = definition->first_hotspot_x;
    int hotspot_y = definition->first_hotspot_y;
    int advance_x = max_x - min_x + 1 + definition->gap;
    int advance_y = max_y - min_y + 1 + definition->gap;

    for (int matrix_y = 0; matrix_y < definition->elements_y; ++matrix_y) {
        for (int matrix_x = 0; matrix_x < definition->elements_x; ++matrix_x) {
            select_region(current_id++);
            set_region_minimum(min_x, min_y);
            set_region_maximum(max_x, max_y);
            set_region_hotspot(hotspot_x, hotspot_y);
            min_x += advance_x;
            max_x += advance_x;
            hotspot_x += advance_x;
        }
        min_y += advance_y;
        max_y += advance_y;
        hotspot_y += advance_y;
        min_x = definition->first_min_x;
        max_x = definition->first_max_x;
        hotspot_x = definition->first_hotspot_x;
    }
}

void draw_region_at(int drawing_x, int drawing_y)
{
    set_drawing_point(drawing_x, drawing_y);
    vircon_gpu_draw_region();
}
