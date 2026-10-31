/* Frame-based button repeat for the benchmark's workload count. */
#ifndef C_BOIDS_COUNT_REPEAT_H
#define C_BOIDS_COUNT_REPEAT_H

typedef struct CountRepeat {
    int direction;
    unsigned start_frame;
    unsigned emitted_repeats;
} CountRepeat;

/* Return signed count steps due at this frame, including the initial press. */
int count_repeat_steps(CountRepeat *state, int direction, unsigned frame);

#endif
