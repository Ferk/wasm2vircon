#include <vircon.h>

/* Ordinary-C port of ConsoleSoftware/TestPrograms/Test-BlendingModes.
 * The upstream program's inline-assembly header calls are represented by the
 * corresponding project runtime API; its scene layout and update sequence are
 * intentionally unchanged. */

enum {
    REGION_BACKGROUND = 0,
    REGION_BALL = 1,
    REGION_LAMP = 2,
    REGION_LIGHT = 10,
    REGION_SHADOW = 20,
    REGION_REFLECTION = 30,
    SOUND_LAMP_MOVING = 0,
    SCENE_FRAMES = 4 * 60,
    BLENDING_ALPHA = 0x20,
    BLENDING_ADD = 0x21,
    BLENDING_SUBTRACT = 0x22
};

static int make_gray(int brightness)
{
    return 0xFF000000 | (brightness << 16) | (brightness << 8) | brightness;
}

int main(void)
{
    int elapsed_frames = 0;

    select_texture(0);
    select_region(REGION_BACKGROUND);
    set_region_minimum(0, 0); set_region_maximum(639, 359); set_region_hotspot(0, 0);
    select_region(REGION_BALL);
    set_region_minimum(202, 361); set_region_maximum(301, 460); set_region_hotspot(251, 460);
    select_region(REGION_LAMP);
    set_region_minimum(202, 462); set_region_maximum(272, 592); set_region_hotspot(237, 557);
    select_region(REGION_SHADOW);
    set_region_minimum(303, 361); set_region_maximum(434, 413); set_region_hotspot(303, 386);
    select_region(REGION_LIGHT);
    set_region_minimum(1, 361); set_region_maximum(200, 607); set_region_hotspot(101, 414);
    select_region(REGION_REFLECTION);
    set_region_minimum(303, 415); set_region_maximum(316, 431); set_region_hotspot(309, 425);

    select_sound(SOUND_LAMP_MOVING);
    set_sound_loop(1);
    play_sound_in_channel(SOUND_LAMP_MOVING, 0);
    set_channel_volume(0.4f);

    for (;;) {
        float oscillation_angle = (6.28318530718f * (float)elapsed_frames / (float)SCENE_FRAMES) + 0.52359877559f;
        int lamp_offset_x = (int)(40.0f * cosf(oscillation_angle));
        float lamp_flicker = 0.5f * (float)(rand() % 5) / 5.0f;
        float shadow_scale_x = 1.0f + 0.2f * (1.0f - cosf(oscillation_angle));

        set_blending_mode(BLENDING_ALPHA);
        select_region(REGION_BACKGROUND);
        draw_region_at(0, 0);

        set_blending_mode(BLENDING_SUBTRACT);
        set_multiply_color(make_gray(130 + (int)(20.0f * cosf(oscillation_angle))));
        set_drawing_scale(shadow_scale_x, 1.0f);
        select_region(REGION_SHADOW);
        set_drawing_point(432, 254);
        draw_region_zoomed();

        set_blending_mode(BLENDING_ALPHA);
        set_multiply_color(0xFFFFFFFF);
        select_region(REGION_BALL);
        draw_region_at(469, 254);

        select_region(REGION_LAMP);
        draw_region_at(206 + lamp_offset_x, 95);

        set_blending_mode(BLENDING_ADD);
        set_multiply_color(make_gray((int)(255.0f * (1.0f - lamp_flicker))));
        select_region(REGION_LIGHT);
        draw_region_at(206 + lamp_offset_x, 95);

        set_multiply_color(0xAAFFFFFF);
        select_region(REGION_REFLECTION);
        draw_region_at(438 + lamp_offset_x / 10, 191 - lamp_offset_x / 6);

        ++elapsed_frames;
        if (elapsed_frames >= SCENE_FRAMES) elapsed_frames = 0;
        end_frame();
    }
}
