#include <vircon.h>

/* Ordinary-C port of ConsoleSoftware/TestPrograms/Test-Rotozoom. */

static int clamp_int(int value, int minimum, int maximum)
{
    if (value < minimum) value = minimum;
    if (value > maximum) value = maximum;
    return value;
}

static float clamp_float(float value, float minimum, float maximum)
{
    if (value < minimum) value = minimum;
    if (value > maximum) value = maximum;
    return value;
}

int main(void)
{
    int rotation_degrees = 0;
    float zoom_factor = 1.0f;

    select_texture(0);
    select_region(10);
    set_region_minimum(0, 0); set_region_maximum(639, 359); set_region_hotspot(0, 0);

    select_texture(1);
    select_region(20);
    set_region_minimum(0, 0); set_region_maximum(205, 164); set_region_hotspot(57, 82);

    select_gamepad(0);
    for (;;) {
        if (gamepad_left() > 0) --rotation_degrees;
        if (gamepad_right() > 0) ++rotation_degrees;
        if (gamepad_up() > 0) zoom_factor += 0.025f;
        if (gamepad_down() > 0) zoom_factor -= 0.025f;

        rotation_degrees = clamp_int(rotation_degrees, -360, 360);
        zoom_factor = clamp_float(zoom_factor, -2.5f, 2.5f);

        set_drawing_scale(zoom_factor, zoom_factor);
        set_drawing_angle((float)rotation_degrees * 0.01745329252f);

        set_multiply_color(0xFFFFFFFF);
        select_texture(0); select_region(10); draw_region_at(0, 0);

        set_multiply_color(0x80FFFFFF);
        select_texture(1); select_region(20); draw_region_rotozoomed_at(200, 120);

        set_multiply_color(0xFFFFFFFF);
        print_at(450, 310, "Angle:");
        print_int_at(520, 310, rotation_degrees);
        print_at(570, 310, "degrees");
        print_at(450, 330, "Zoom: X");
        print_fixed_2_at(530, 330, zoom_factor);
        end_frame();
    }
}
