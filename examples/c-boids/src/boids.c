#include <vircon.h>

#include "boids.h"

#define MAX_BOID_COUNT     150
#define INITIAL_BOID_COUNT 50
#define BOID_COUNT_STEP    10

#define WIDTH  640.0f
#define HEIGHT 360.0f

#define VIEW_RANGE       60.0f
#define AVOID_RANGE      20.0f

#define CENTERING_FACTOR 0.005f
#define MATCHING_FACTOR  0.05f
#define AVOID_FACTOR     0.05f

#define MAX_SPEED        4.0f
#define MIN_SPEED        2.0f
#define MARGIN           50.0f
#define TURN_FACTOR      0.2f


typedef struct
{
    float x;
    float y;

    float vx;
    float vy;
} Boid;


Boid boids[MAX_BOID_COUNT];
static int boid_count = INITIAL_BOID_COUNT;


/* ---------------------------------------------------------
   Tiny deterministic random generator
   --------------------------------------------------------- */

static unsigned int random_state = 1;

unsigned int random_u32()
{
    random_state =
        random_state * 1664525u + 1013904223u;

    return random_state;
}


float random_float(float min, float max)
{
    unsigned int value = random_u32() & 0xFFFF;

    float t = (float)value / 65535.0f;

    return min + (max - min) * t;
}


/* ---------------------------------------------------------
   Initialize
   --------------------------------------------------------- */

/* Initialize one newly enabled boid from the deterministic local generator. */
static void initialize_boid(int index)
{
    boids[index].x = random_float(100.0f, WIDTH - 100.0f);
    boids[index].y = random_float(100.0f, HEIGHT - 100.0f);
    boids[index].vx = random_float(-2.0f, 2.0f);
    boids[index].vy = random_float(-2.0f, 2.0f);
}

/* Initialize the visible starting workload. */
void initialize_boids(void)
{
    int index;

    for (index = 0; index < boid_count; ++index)
        initialize_boid(index);
}

/* Adds one fixed batch, stopping at the benchmark's fixed capacity. */
void increase_boid_count(void)
{
    int target = boid_count + BOID_COUNT_STEP;

    if (target > MAX_BOID_COUNT)
        target = MAX_BOID_COUNT;
    while (boid_count < target) {
        initialize_boid(boid_count);
        ++boid_count;
    }
}

/* Removes one batch while retaining at least one visible simulated boid. */
void decrease_boid_count(void)
{
    if (boid_count > BOID_COUNT_STEP)
        boid_count -= BOID_COUNT_STEP;
    else
        boid_count = 10;
}

/* Returns the active workload rather than the array's maximum capacity. */
int get_boid_count(void)
{
    return boid_count;
}


/* ---------------------------------------------------------
   Update one simulation step
   --------------------------------------------------------- */

void update_boids()
{
    for(int i = 0; i < boid_count; i++)
    {
        float center_x = 0;
        float center_y = 0;

        float avg_vx = 0;
        float avg_vy = 0;

        float avoid_x = 0;
        float avoid_y = 0;

        int neighbors = 0;


        /*
         * Compare this boid against every other boid.
         *
         * This deliberately uses the simple O(N²) algorithm.
         * That's useful for a benchmark.
         */

        for(int j = 0; j < boid_count; j++)
        {
            if(i == j)
                continue;

            float dx = boids[i].x - boids[j].x;
            float dy = boids[i].y - boids[j].y;

            float distance_squared =
                dx * dx + dy * dy;


            /* Cohesion + alignment */

            if(distance_squared <
               VIEW_RANGE * VIEW_RANGE)
            {
                center_x += boids[j].x;
                center_y += boids[j].y;

                avg_vx += boids[j].vx;
                avg_vy += boids[j].vy;

                neighbors++;
            }


            /* Separation */

            if(distance_squared <
               AVOID_RANGE * AVOID_RANGE)
            {
                avoid_x += dx;
                avoid_y += dy;
            }
        }


        if(neighbors > 0)
        {
            center_x /= neighbors;
            center_y /= neighbors;

            avg_vx /= neighbors;
            avg_vy /= neighbors;


            /* Cohesion */

            boids[i].vx +=
                (center_x - boids[i].x)
                * CENTERING_FACTOR;

            boids[i].vy +=
                (center_y - boids[i].y)
                * CENTERING_FACTOR;


            /* Alignment */

            boids[i].vx +=
                (avg_vx - boids[i].vx)
                * MATCHING_FACTOR;

            boids[i].vy +=
                (avg_vy - boids[i].vy)
                * MATCHING_FACTOR;
        }


        /* Separation */

        boids[i].vx +=
            avoid_x * AVOID_FACTOR;

        boids[i].vy +=
            avoid_y * AVOID_FACTOR;


        /* Keep flock inside screen */

        if(boids[i].x < MARGIN)
            boids[i].vx += TURN_FACTOR;

        if(boids[i].x > WIDTH - MARGIN)
            boids[i].vx -= TURN_FACTOR;

        if(boids[i].y < MARGIN)
            boids[i].vy += TURN_FACTOR;

        if(boids[i].y > HEIGHT - MARGIN)
            boids[i].vy -= TURN_FACTOR;


        /*
         * Cheap speed limiter.
         *
         * Deliberately avoids sqrt(), making this easier
         * to port to restricted WASM targets.
         */

        float speed_squared =
            boids[i].vx * boids[i].vx +
            boids[i].vy * boids[i].vy;

        if(speed_squared > MAX_SPEED * MAX_SPEED)
        {
            boids[i].vx *= 0.9f;
            boids[i].vy *= 0.9f;
        }

        if(speed_squared < MIN_SPEED * MIN_SPEED)
        {
            boids[i].vx *= 1.1f;
            boids[i].vy *= 1.1f;
        }


        /* Move */

        boids[i].x += boids[i].vx;
        boids[i].y += boids[i].vy;
    }
}

/* Draw each simulated point as a small cyan BIOS-pixel square. The simulation
 * stays independent from rendering; this is deliberately just a visual view
 * of the O(N^2) workload. */
void draw_boids(void)
{
    int i;

    select_texture(-1);
    select_region(256);
    set_multiply_color(color_cyan);
    set_drawing_scale(3.0f, 3.0f);

    for (i = 0; i < boid_count; ++i) {
        set_drawing_point((int)boids[i].x, (int)boids[i].y);
        draw_region_zoomed();
    }

    set_drawing_scale(1.0f, 1.0f);
    set_multiply_color(color_white);
}

/* Runs the complete interactive benchmark. Input, timing and presentation
 * live beside the simulation so main.c remains only the cartridge entry. */
void run_boids(void)
{
    int update_start_frame;
    int update_frames;
    int up_pressed;
    int down_pressed;
    int previous_up_pressed = 0;
    int previous_down_pressed = 0;
    float frames_per_update;

    initialize_boids();

    for (;;) {
        /* Change workload once per press rather than every held frame. */
        select_gamepad(0);
        /* Input ports may use signed nonzero transition states. */
        up_pressed = gamepad_up() > 0;
        down_pressed = gamepad_down() > 0;
        if (up_pressed && !previous_up_pressed)
            increase_boid_count();
        else if (down_pressed && !previous_down_pressed)
            decrease_boid_count();
        previous_up_pressed = up_pressed;
        previous_down_pressed = down_pressed;

        update_start_frame = get_frame_counter();
        update_boids();
        /* A sub-frame update still occupies one displayed frame. */
        update_frames = get_frame_counter() - update_start_frame + 1;
        frames_per_update = (float)frames_per_second / (float)update_frames;

        /* Retain the previous flock while an O(N^2) update spans a frame. */
        clear_screen(color_black);
        draw_boids();
        print_at(10, 10, "Boids:");
        print_uint_at(80, 10, (unsigned)get_boid_count());
        print_at(120, 10, "(up/down +/-10)");
        print_at(10, 30, "Update frames:");
        print_uint_at(160, 30, (unsigned)update_frames);
        print_at(10, 50, "FPS:");
        print_fixed_2_at(60, 50, frames_per_update);
        end_frame();
    }
}
