#include <vircon.h>

/* Exercises all pure colour packing/extraction helpers and public constants. */
int main(void)
{
    int seed = get_frame_counter();
    int rgba = make_color_rgba(seed, seed + 1, seed + 2, seed + 3);
    int rgb = make_color_rgb(seed + 4, seed + 5, seed + 6);
    int gray = make_gray(seed + 7);

    return rgba ^ rgb ^ gray ^ get_color_red(rgba) ^ get_color_green(rgba) ^
           get_color_blue(rgba) ^ get_color_alpha(rgba) ^ screen_width ^
           screen_height ^ color_red ^ color_green ^ color_blue;
}
