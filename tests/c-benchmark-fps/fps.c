/* Check FPS from completed frames, including slow draws and counter wrap. */
#include "fps_counter.h"

int main(void)
{
    FpsCounter state;
    fps_counter_reset(&state, 100);
    for (unsigned i = 1; i <= 60; ++i)
        fps_counter_complete(&state, 100 + i);
    if (state.hundredths != 6000) return 1;

    /* One completed update/draw per two display frames must report 30 FPS. */
    fps_counter_reset(&state, 200);
    for (unsigned i = 1; i <= 30; ++i)
        fps_counter_complete(&state, 200 + i * 2);
    if (state.hundredths != 3000) return 2;

    /* Mixtures of fast and slow frames produce the measured average. */
    fps_counter_reset(&state, 300);
    for (unsigned i = 1; i <= 30; ++i)
        fps_counter_complete(&state, 300 + i);
    for (unsigned i = 1; i <= 15; ++i)
        fps_counter_complete(&state, 330 + i * 2);
    if (state.hundredths != 4500) return 3;

    fps_counter_reset(&state, 0xfffffff0u);
    for (unsigned i = 1; i <= 30; ++i)
        fps_counter_complete(&state, 0xfffffff0u + i * 2);
    if (state.hundredths != 3000) return 4;
    return 0;
}
