/* Independent moving sprites for the resource-free BunnyMark-style workload. */
#include <vircon.h>

#include "bunnies.h"

enum { MAX_BUNNIES = 9999, INITIAL_BUNNIES = 100, PREPARED_BUNNIES = 500 };

typedef struct Bunny {
    float x, y, dx, dy;
} Bunny;

static Bunny bunnies[MAX_BUNNIES];
static int bunny_count;
static int initialized_bunnies;

/* Assign reproducible positions and velocities without using hardware RNG. */
static void initialize_bunny(int index)
{
    bunnies[index].x = (float)(10 + (index * 13) % 600);
    bunnies[index].y = (float)(20 + (index * 17) % 300);
    bunnies[index].dx = (float)((index & 7) - 3) * 0.5f;
    bunnies[index].dy = (float)((index & 3) - 2) * 0.5f;
}

/* Prepare the same initial state for both deterministic profiling counts. */
void reset_bunnies(void)
{
    for (int i = 0; i < PREPARED_BUNNIES; ++i)
        initialize_bunny(i);
    initialized_bunnies = PREPARED_BUNNIES;
    bunny_count = INITIAL_BUNNIES;
}

/* Initialize additional bunnies only when the selected count needs them. */
void set_bunny_count(int count)
{
    if (count < 0)
        count = 0;
    if (count > MAX_BUNNIES)
        count = MAX_BUNNIES;
    while (initialized_bunnies < count) {
        initialize_bunny(initialized_bunnies);
        ++initialized_bunnies;
    }
    bunny_count = count;
}

/* Return the current number of active sprites. */
int get_bunny_count(void) { return bunny_count; }

/* Move each active sprite and reverse its velocity at the screen edges. */
void update_bunnies(void)
{
    for (int i = 0; i < bunny_count; ++i) {
        Bunny *bunny = &bunnies[i];
        bunny->x += bunny->dx;
        bunny->y += bunny->dy;
        if (bunny->x < 0.0f || bunny->x > 629.0f)
            bunny->dx = -bunny->dx;
        if (bunny->y < 0.0f || bunny->y > 339.0f)
            bunny->dy = -bunny->dy;
    }
}

/* Draw a glyph at each sprite position; no cartridge texture is required. */
void draw_bunnies(void)
{
    select_texture(-1);
    select_region('b');
    set_multiply_color(color_gray);
    for (int i = 0; i < bunny_count; ++i)
        draw_region_at((int)bunnies[i].x, (int)bunnies[i].y);
    set_multiply_color(color_white);
}
