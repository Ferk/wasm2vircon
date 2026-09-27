// Flocking state, simulation and flock rendering for the C++ boids demo.
#include <vircon.h>

#include "boids.hpp"

namespace {

constexpr int max_boid_count = 150;
constexpr int initial_boid_count = 50;
constexpr int boid_count_step = 10;
constexpr float width = 640.0f;
constexpr float height = 360.0f;
constexpr float view_range = 60.0f;
constexpr float avoid_range = 20.0f;
constexpr float centering_factor = 0.005f;
constexpr float matching_factor = 0.05f;
constexpr float avoid_factor = 0.05f;
constexpr float max_speed = 4.0f;
constexpr float min_speed = 2.0f;
constexpr float margin = 50.0f;
constexpr float turn_factor = 0.2f;

struct Boid {
    float x;
    float y;
    float vx;
    float vy;
};

Boid flock[max_boid_count];
int active_boid_count = initial_boid_count;
unsigned random_state = 1;

// Advances the benchmark's small deterministic pseudo-random generator.
unsigned random_u32()
{
    random_state = random_state * 1664525u + 1013904223u;
    return random_state;
}

// Produces a deterministic floating-point value in the requested interval.
float random_float(float minimum, float maximum)
{
    const unsigned value = random_u32() & 0xffffu;
    const float fraction = static_cast<float>(value) / 65535.0f;
    return minimum + (maximum - minimum) * fraction;
}

// Initializes one flock member with deterministic position and velocity.
void initialize_boid(int index)
{
    flock[index].x = random_float(100.0f, width - 100.0f);
    flock[index].y = random_float(100.0f, height - 100.0f);
    flock[index].vx = random_float(-2.0f, 2.0f);
    flock[index].vy = random_float(-2.0f, 2.0f);
}

} // namespace

namespace boids {

// Initializes the initial visible benchmark workload.
void initialize()
{
    for (int index = 0; index < active_boid_count; ++index)
        initialize_boid(index);
}

// Adds one batch of work without exceeding the fixed flock capacity.
void increase()
{
    int target = active_boid_count + boid_count_step;
    if (target > max_boid_count)
        target = max_boid_count;
    while (active_boid_count < target) {
        initialize_boid(active_boid_count);
        ++active_boid_count;
    }
}

// Removes one batch while retaining a visible flock.
void decrease()
{
    if (active_boid_count > boid_count_step)
        active_boid_count -= boid_count_step;
    else
        active_boid_count = 10;
}

// Returns the active workload rather than the maximum array capacity.
int count()
{
    return active_boid_count;
}

// Runs one deliberately simple O(N²) flocking simulation step.
void update()
{
    for (int i = 0; i < active_boid_count; ++i) {
        float center_x = 0.0f;
        float center_y = 0.0f;
        float average_vx = 0.0f;
        float average_vy = 0.0f;
        float avoid_x = 0.0f;
        float avoid_y = 0.0f;
        int neighbors = 0;

        for (int j = 0; j < active_boid_count; ++j) {
            if (i == j)
                continue;
            const float dx = flock[i].x - flock[j].x;
            const float dy = flock[i].y - flock[j].y;
            const float distance_squared = dx * dx + dy * dy;

            if (distance_squared < view_range * view_range) {
                center_x += flock[j].x;
                center_y += flock[j].y;
                average_vx += flock[j].vx;
                average_vy += flock[j].vy;
                ++neighbors;
            }
            if (distance_squared < avoid_range * avoid_range) {
                avoid_x += dx;
                avoid_y += dy;
            }
        }

        Boid& current = flock[i];
        if (neighbors > 0) {
            const float neighbor_count = static_cast<float>(neighbors);
            center_x /= neighbor_count;
            center_y /= neighbor_count;
            average_vx /= neighbor_count;
            average_vy /= neighbor_count;
            current.vx += (center_x - current.x) * centering_factor;
            current.vy += (center_y - current.y) * centering_factor;
            current.vx += (average_vx - current.vx) * matching_factor;
            current.vy += (average_vy - current.vy) * matching_factor;
        }

        current.vx += avoid_x * avoid_factor;
        current.vy += avoid_y * avoid_factor;
        if (current.x < margin) current.vx += turn_factor;
        if (current.x > width - margin) current.vx -= turn_factor;
        if (current.y < margin) current.vy += turn_factor;
        if (current.y > height - margin) current.vy -= turn_factor;

        const float speed_squared = current.vx * current.vx + current.vy * current.vy;
        if (speed_squared > max_speed * max_speed) {
            current.vx *= 0.9f;
            current.vy *= 0.9f;
        }
        if (speed_squared < min_speed * min_speed) {
            current.vx *= 1.1f;
            current.vy *= 1.1f;
        }
        current.x += current.vx;
        current.y += current.vy;
    }
}

// Draws every active flock member as a small cyan BIOS-pixel square.
void draw()
{
    select_texture(-1);
    select_region(256);
    set_multiply_color(color_cyan);
    set_drawing_scale(3.0f, 3.0f);
    for (int index = 0; index < active_boid_count; ++index) {
        set_drawing_point(static_cast<int>(flock[index].x), static_cast<int>(flock[index].y));
        draw_region_zoomed();
    }
    set_drawing_scale(1.0f, 1.0f);
    set_multiply_color(color_white);
}

} // namespace boids
