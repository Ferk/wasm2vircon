/* Interface for the independent BunnyMark-style workload. */
#ifndef C_BOIDS_BUNNIES_H
#define C_BOIDS_BUNNIES_H

/* Reset the deterministic BunnyMark-style workload. */
void reset_bunnies(void);
/* Select a number of bunnies within the available capacity. */
void set_bunny_count(int count);
/* Return the number of active bunnies. */
int get_bunny_count(void);
/* Advance all active bunnies by one simulation step. */
void update_bunnies(void);
/* Draw the bunnies using the BIOS font's lowercase b glyph. */
void draw_bunnies(void);

#endif
