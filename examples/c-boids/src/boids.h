/* Interface for the independent flocking workload. */
#ifndef C_BOIDS_BOIDS_H
#define C_BOIDS_BOIDS_H

/* Reset the flock and its deterministic random state. */
void reset_boids(void);
/* Select the number of active boids, clamped to the available capacity. */
void set_boid_count(int count);
/* Return the current number of active boids. */
int get_boid_count(void);
/* Advance the flock by one simulation step. */
void update_boids(void);
/* Draw the current flock using BIOS pixels. */
void draw_boids(void);

#endif
