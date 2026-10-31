/* Alpha Swarm: large, translucent BIOS glyphs bouncing and rotating. */
#include <vircon.h>

#include "alpha_swarm.h"

enum { INITIAL_LAYERS = 4, MAX_LAYERS = 96, GLYPH_REGION = 257 };

/* A 10x20 glyph at 10x scale fits within a radius of 114 pixels at any angle.
 * The region uses a centered hotspot; y=80 keeps the glyphs below the HUD. */
enum { LEFT = 114, RIGHT = 526, TOP = 194, BOTTOM = 246 };

typedef struct Particle {
    int x, y, dx, dy;
    float angle;
} Particle;

static int layer_count;
static Particle particles[MAX_LAYERS];

/* The BIOS 'v' points down at angle zero; align it with the velocity. */
static float particle_angle(const Particle *particle)
{
    return vircon__cpu_atan2((float)particle->dy, (float)particle->dx) - pi / 2.0f;
}

/* Restore the same initial workload whenever this mode is selected. */
void reset_alpha_swarm(void)
{
    int glyph_x, glyph_y;
    layer_count = INITIAL_LAYERS;
    for (int i = 0; i < MAX_LAYERS; ++i) {
        Particle *particle = &particles[i];
        particle->x = LEFT + (i * 83) % (RIGHT - LEFT + 1);
        particle->y = TOP + (i * 29) % (BOTTOM - TOP + 1);
        particle->dx = (i & 1) ? 3 : -3;
        particle->dy = (i & 2) ? 2 : -2;
        particle->angle = particle_angle(particle);
    }
    /* Read the BIOS-selected glyph location: different BIOS images place the
     * font in different parts of their texture. Keep text's region unchanged. */
    select_texture(-1);
    select_region('v');
    get_region_minimum(&glyph_x, &glyph_y);
    select_region(GLYPH_REGION);
    define_region(glyph_x, glyph_y, glyph_x + 9, glyph_y + 19, glyph_x + 5, glyph_y + 10);
}

/* Clamp the selected count to a small, GPU-saturating range. */
void set_alpha_swarm_count(int count)
{
    if (count < 0) count = 0;
    if (count > MAX_LAYERS) count = MAX_LAYERS;
    layer_count = count;
}

/* Return the selected layer count. */
int get_alpha_swarm_count(void) { return layer_count; }

/* Bounce inside the drawable area and update the angle on each impact. */
void update_alpha_swarm(void)
{
    for (int i = 0; i < layer_count; ++i) {
        Particle *particle = &particles[i];
        int bounced = 0;
        particle->x += particle->dx;
        particle->y += particle->dy;
        if (particle->x < LEFT || particle->x > RIGHT) {
            particle->x = particle->x < LEFT ? LEFT : RIGHT;
            particle->dx = -particle->dx;
            bounced = 1;
        }
        if (particle->y < TOP || particle->y > BOTTOM) {
            particle->y = particle->y < TOP ? TOP : BOTTOM;
            particle->dy = -particle->dy;
            bounced = 1;
        }
        if (bounced)
            particle->angle = particle_angle(particle);
    }
}

/* Draw centered scaled and rotated glyphs, then restore shared GPU state. */
void draw_alpha_swarm(void)
{
    static const int colors[] = {0x70FF4030, 0x7040D060, 0x703060FF, 0x70E0C040};
    select_texture(-1);
    select_region(GLYPH_REGION);
    set_blending_mode(blending_alpha);
    set_drawing_scale(10.0f, 10.0f);
    for (int i = 0; i < layer_count; ++i) {
        Particle *particle = &particles[i];
        set_multiply_color(colors[i & 3]);
        set_drawing_angle(particle->angle);
        draw_region_rotozoomed_at(particle->x, particle->y);
    }
    set_drawing_scale(1.0f, 1.0f);
    set_drawing_angle(0.0f);
    set_multiply_color(color_white);
}
