/* Interface for the large translucent rotating-glyph GPU workload. */
#ifndef C_BOIDS_ALPHA_SWARM_H
#define C_BOIDS_ALPHA_SWARM_H

/* Reset the layer count and animation state. */
void reset_alpha_swarm(void);
/* Change the number of layers, clamped to the supported range. */
void set_alpha_swarm_count(int count);
/* Return the selected number of layers. */
int get_alpha_swarm_count(void);
/* Advance the deterministic layer animation. */
void update_alpha_swarm(void);
/* Draw the layers and restore the shared drawing scale and color. */
void draw_alpha_swarm(void);

#endif
