/* Measure completed benchmark iterations against Vircon32 display frames. */
#ifndef C_BOIDS_FPS_COUNTER_H
#define C_BOIDS_FPS_COUNTER_H

typedef struct FpsCounter {
    unsigned start_frame;
    unsigned completed;
    unsigned hundredths;
} FpsCounter;

/* Start a fresh measurement window at the supplied display frame. */
void fps_counter_reset(FpsCounter *counter, unsigned frame);

/* Record a completed iteration and update the FPS after at least 60 frames. */
void fps_counter_complete(FpsCounter *counter, unsigned frame);

#endif
