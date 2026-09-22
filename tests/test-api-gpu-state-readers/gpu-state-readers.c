#include <vircon.h>

static int point_x;
static int point_y;
static float scale_x;
static float scale_y;

/* Exercises all public GPU-state readers, including typed f32 port values. */
int main(void)
{
    int result = get_multiply_color();
    get_drawing_point(&point_x, &point_y);
    get_drawing_scale(&scale_x, &scale_y);
    (void)get_drawing_angle();
    return result + get_blending_mode() + point_x + point_y;
}
