/* Check the public GPU budget reader against the effect of a clear command. */
#include <vircon.h>

/* A clear must consume some of the current frame's available GPU pixels. */
int main(void)
{
    int before = get_gpu_remaining_pixels();
    clear_screen(color_black);
    int after = get_gpu_remaining_pixels();
    return before != gpu_pixels_per_frame || after < 0 || after >= before;
}
