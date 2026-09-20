#include <vircon.h>

/* Ordinary-C port of ConsoleSoftware/TestPrograms/Test-LargeZoom. */

static int clamp_int(int value, int minimum, int maximum)
{
    if (value < minimum) value = minimum;
    if (value > maximum) value = maximum;
    return value;
}

int main(void)
{
    int rotation_degrees = 0;
    int zoom_factor = 20;
    int mirror_x = 1;
    int mirror_y = 1;

    select_texture(0);
    select_region(10);
    set_region_minimum(1, 1);
    set_region_maximum(16, 9);
    set_region_hotspot(1, 1);
    select_gamepad(0);

    for (;;) {
        if (gamepad_left() > 0) --rotation_degrees;
        if (gamepad_right() > 0) ++rotation_degrees;
        if (gamepad_up() == 1) ++zoom_factor;
        /* The upstream program deliberately has this check twice. */
        if (gamepad_down() == 1) --zoom_factor;
        if (gamepad_down() == 1) --zoom_factor;
        if (gamepad_button_a() == 1) mirror_x *= -1;
        if (gamepad_button_b() == 1) mirror_y *= -1;

        /* Returning the clamped value is the equivalent ISO-C adaptation of
         * the upstream pointer helper and avoids linker stack-global baggage. */
        rotation_degrees = clamp_int(rotation_degrees, -360, 360);
        zoom_factor = clamp_int(zoom_factor, 5, 35);

        set_drawing_scale((float)(zoom_factor * mirror_x),
                          (float)(zoom_factor * mirror_y));
        set_drawing_angle((float)rotation_degrees * 0.01745329251f);

        clear_screen(0xFF000000);
        select_texture(0);
        select_region(10);
        draw_region_rotozoomed_at(250, 140);

        set_multiply_color(0x5000FFFF);
        draw_bios_horizontal_line(0, 140, 639);
        draw_bios_vertical_line(250, 0, 359);
        set_multiply_color(0xFFFFFFFF);

        /* Keep the official information, using the existing compact numeric
         * runtime instead of its word-addressed strcpy/itoa/strcat helpers. */
        print_at(10, 280, "Angle:");
        print_int_at(80, 280, rotation_degrees);
        print_at(130, 280, "degrees");
        print_at(10, 300, "Zoom: X");
        print_int_at(80, 300, zoom_factor);
        print_at(10, 320, mirror_x > 0 ? "Mirror X: OFF" : "Mirror X: ON");
        print_at(10, 340, mirror_y > 0 ? "Mirror Y: OFF" : "Mirror Y: ON");
        end_frame();
    }
}
