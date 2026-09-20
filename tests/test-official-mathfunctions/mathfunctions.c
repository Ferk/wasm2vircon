#include <vircon.h>

/* Ordinary-C port of ConsoleSoftware/TestPrograms/Test-MathFunctions.
 * The official compiler uses a mutable global. This port keeps the selected
 * tab in main's local state so the Wasm module needs no linker stack global. */

static float cubic(float x) { return powf(x, 3.0f) - 4.0f * x; }
static void point(float x, float y)
{
    set_drawing_point((int)x, (int)y);
    draw_region_at((int)x, (int)y);
}

static void draw_function(int selected_tab)
{
    float x;
    select_region(3);
    if (selected_tab == 0) for (x=-4; x<=4; x+=.0625f) point(460+40*x,180-40*sinf(x));
    else if (selected_tab == 1) for (x=-4; x<=4; x+=.0625f) point(460+40*x,180-40*cosf(x));
    else if (selected_tab == 2) for (x=-4; x<=4; x+=.0625f) point(460+40*x,180-40*tanf(x));
    else if (selected_tab == 3) for (x=-1; x<=1; x+=.0625f) point(460+40*x,180-40*asinf(x));
    else if (selected_tab == 4) for (x=-1; x<=1; x+=.0625f) point(460+40*x,180-40*acosf(x));
    else if (selected_tab == 5) for (x=-4; x<=4; x+=.0625f) point(460+40*x,180-40*expf(x));
    else if (selected_tab == 6) for (x=4; x>0; x-=.0625f) point(460+40*x,180-40*logf(x));
    else for (x=-4; x<=4; x+=.0625f) point(460+40*x,180-40*cubic(x));
}

int main(void)
{
    int selected_tab = 0;

    select_texture(0);

    select_region(0);
    set_region_minimum(0, 0); set_region_maximum(639, 359); set_region_hotspot(0, 0);
    select_region(1);
    set_region_minimum(1, 361); set_region_maximum(207, 397); set_region_hotspot(1, 361);
    select_region(2);
    set_region_minimum(209, 361); set_region_maximum(233, 385); set_region_hotspot(233, 373);
    select_region(3);
    set_region_minimum(235, 361); set_region_maximum(239, 365); set_region_hotspot(237, 363);

    select_gamepad(0);
    for (;;) {
        set_multiply_color(0xFFFFFFFF);
        select_region(0); draw_region_at(0, 0);
        select_region(1); draw_region_at(59, 22 + 40 * selected_tab);
        select_region(2); draw_region_at(45, 40 + 40 * selected_tab);
        draw_function(selected_tab);

        for (;;) {
            end_frame();
            if (gamepad_up() == 1) {
                if (selected_tab > 0) --selected_tab;
                break;
            }
            if (gamepad_down() == 1) {
                if (selected_tab < 7) ++selected_tab;
                break;
            }
        }
    }
}
