/* Converts held input and elapsed display frames into workload-count steps. */
#include "count_repeat.h"

enum { REPEAT_DELAY_FRAMES = 18, REPEAT_INTERVAL_FRAMES = 6, MAX_CATCHUP_STEPS = 100 };

/* Repeat after 0.3 seconds, then every 0.1 seconds at Vircon32's 60 Hz. */
int count_repeat_steps(CountRepeat *state, int direction, unsigned frame)
{
    unsigned elapsed, repeats, due;
    if (direction == 0) {
        state->direction = 0;
        return 0;
    }
    if (direction != state->direction) {
        state->direction = direction;
        state->start_frame = frame;
        state->emitted_repeats = 0;
        return direction;
    }
    /* Unsigned subtraction also handles the frame counter wrapping around. */
    elapsed = frame - state->start_frame;
    repeats = elapsed < REPEAT_DELAY_FRAMES ? 0 : 1 + (elapsed - REPEAT_DELAY_FRAMES) / REPEAT_INTERVAL_FRAMES;
    due = repeats - state->emitted_repeats;
    state->emitted_repeats = repeats;
    if (due > MAX_CATCHUP_STEPS)
        due = MAX_CATCHUP_STEPS;
    return direction * (int)due;
}
