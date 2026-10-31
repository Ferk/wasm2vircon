/* Shared controls, timing, presentation, and deterministic benchmark driver. */
#include <vircon.h>

#include "benchmark.h"
#include "alpha_swarm.h"
#include "boids.h"
#include "bunnies.h"
#include "count_repeat.h"
#include "fps_counter.h"

enum { CYCLES_PER_FRAME = 250000, TOUR_FRAMES = 60, MODE_BOIDS = 0, MODE_BUNNIES = 1, MODE_ALPHA = 2 };

typedef struct BenchmarkCase {
    int mode;
    int count;
} BenchmarkCase;

/* Low and high loads exercise each workload in a predictable order. */
static const BenchmarkCase tour_cases[] = {
    {MODE_BOIDS, 10}, {MODE_BOIDS, 50},
    {MODE_BUNNIES, 500}, {MODE_BUNNIES, 2500},
    {MODE_ALPHA, 4}, {MODE_ALPHA, 64}
};
static int tour_case;

/* Reset one workload whenever it becomes active. */
static void reset_workload(int mode)
{
    if (mode == MODE_BOIDS)
        reset_boids();
    else if (mode == MODE_BUNNIES)
        reset_bunnies();
    else
        reset_alpha_swarm();
}

/* Read the current workload size. */
static int workload_count(int mode)
{
    if (mode == MODE_BOIDS) return get_boid_count();
    if (mode == MODE_BUNNIES) return get_bunny_count();
    return get_alpha_swarm_count();
}

/* Apply a workload-specific count and let each module clamp its capacity. */
static void set_workload_count(int mode, int count)
{
    if (mode == MODE_BOIDS)
        set_boid_count(count);
    else if (mode == MODE_BUNNIES)
        set_bunny_count(count);
    else
        set_alpha_swarm_count(count);
}

/* Dim the workload behind the HUD using a scaled translucent BIOS pixel. */
static void draw_hud_background(void)
{
    select_texture(-1);
    select_region(256);
    set_blending_mode(blending_alpha);
    set_multiply_color(0xB0000000);
    set_drawing_scale(380.0f, 70.0f);
    draw_region_zoomed_at(5, 5);
    set_drawing_scale(1.0f, 1.0f);
    set_multiply_color(color_white);
}

/* Draw the shared HUD before or after a workload, as its pixel budget permits. */
static void draw_hud(int mode, int cpu_percent, int gpu_percent, float fps)
{
    draw_hud_background();
    print_at(10, 10, mode == MODE_BOIDS ? "Boids" : (mode == MODE_BUNNIES ? "BunnyMark" : "Alpha Swarm"));
    print_at(230, 10, "Count:");
    print_uint_at(295, 10, (unsigned)workload_count(mode));
    print_at(10, 30, "FPS:");
    print_fixed_2_at(55, 30, fps);
    print_at(120, 30, "GPU%:");
    print_fixed_2_at(175, 30, (float)gpu_percent / 100.0f);
    print_at(245, 30, "CPU%:");
    print_fixed_2_at(300, 30, (float)cpu_percent / 100.0f);
    if (tour_case != 0) {
        print_at(10, 50, "Benchmark tour:");
        print_uint_at(170, 50, (unsigned)tour_case);
        print_at(180, 50, "/6");
    } else {
        print_at(10, 50, "L/R mode  DPAD +/-  START tour");
    }
}

/* Update, draw, and measure one frame using the same HUD for each workload. */
static void benchmark_frame(int mode, int *cpu_percent, int *gpu_percent, FpsCounter *fps_counter)
{
    int start_frame = get_frame_counter();
    int start_cycles = get_cycle_counter();
    int remaining_pixels;

    if (mode == MODE_BOIDS)
        update_boids();
    else if (mode == MODE_BUNNIES)
        update_bunnies();
    else
        update_alpha_swarm();
    clear_screen(color_black);
    if (mode == MODE_ALPHA) {
        /* The layer draws can exhaust the budget. Draw the HUD first and
         * keep the layers below it so the indicators remain visible. */
        draw_hud(mode, *cpu_percent, *gpu_percent, (float)fps_counter->hundredths / 100.0f);
        draw_alpha_swarm();
    } else {
        if (mode == MODE_BOIDS)
            draw_boids();
        else
            draw_bunnies();
        draw_hud(mode, *cpu_percent, *gpu_percent, (float)fps_counter->hundredths / 100.0f);
    }

    /* Percentages are stored in hundredths to retain fractional precision. */
    *cpu_percent = ((get_frame_counter() - start_frame) * CYCLES_PER_FRAME +
                    get_cycle_counter() - start_cycles) / (CYCLES_PER_FRAME / 10000);
    remaining_pixels = get_gpu_remaining_pixels();
    if (remaining_pixels < 0)
        remaining_pixels = 0;
    else if (remaining_pixels > gpu_pixels_per_frame)
        remaining_pixels = gpu_pixels_per_frame;
    int used_pixels = gpu_pixels_per_frame - remaining_pixels;
    int pixels_per_percent = gpu_pixels_per_frame / 100;
    *gpu_percent = (used_pixels / pixels_per_percent) * 100 +
                   (used_pixels % pixels_per_percent) * 100 / pixels_per_percent;
    end_frame();
    fps_counter_complete(fps_counter, (unsigned)get_frame_counter());
}

/* Run fixed workloads and leave a completion marker for headless profiling. */
static void run_tour(void)
{
    int cpu_percent = 0;
    int gpu_percent = 0;
    int completed_cases = 0;
    int checksum = 0;
    FpsCounter fps_counter;
    for (unsigned i = 0; i < sizeof(tour_cases) / sizeof(tour_cases[0]); ++i) {
        int mode = tour_cases[i].mode;
        tour_case = (int)i + 1;
        reset_workload(mode);
        set_workload_count(mode, tour_cases[i].count);
        /* Begin each timed case at a display-frame boundary after setup. */
        end_frame();
        cpu_percent = 0;
        gpu_percent = 0;
        unsigned start_frame = (unsigned)get_frame_counter();
        fps_counter_reset(&fps_counter, start_frame);
        unsigned updates = 0;
        unsigned cpu_total = 0;
        unsigned gpu_total = 0;
        do {
            benchmark_frame(mode, &cpu_percent, &gpu_percent, &fps_counter);
            cpu_total += (unsigned)cpu_percent;
            gpu_total += (unsigned)gpu_percent;
            ++updates;
        } while ((unsigned)get_frame_counter() - start_frame < TOUR_FRAMES);
        /* Publish a checkpoint for simulator tools. Drawing point Y above the
         * screen carries the case and count/1000; X carries count%1000. Colour
         * carries percentage hundredths and scale X carries FPS. */
        unsigned elapsed = (unsigned)get_frame_counter() - start_frame;
        unsigned fps_hundredths = updates * 6000u / elapsed;
        unsigned average_cpu = cpu_total / updates;
        unsigned average_gpu = gpu_total / updates;
        if (average_cpu > 262143u) average_cpu = 262143u;
        set_multiply_color((int)(average_gpu | (average_cpu << 14)));
        set_drawing_scale((float)fps_hundredths / 100.0f, 1.0f);
        int count = workload_count(mode);
        set_drawing_point(count % 1000, 1001 + (int)i + 6 * (count / 1000));
        set_multiply_color(color_white);
        set_drawing_scale(1.0f, 1.0f);
        checksum += (int)(i + 1) * (workload_count(mode) + 1);
        ++completed_cases;
    }
    /* GPU drawing coordinates are clamped, so keep the checksum in range. */
    set_drawing_point(checksum % 997, completed_cases);
    tour_case = 0;
}

/* Run the interactive controller; Start launches the deterministic tour. */
void run_benchmark(void)
{
    int mode = MODE_BOIDS;
    int cpu_percent = 0;
    int gpu_percent = 0;
    int prev_left = 0, prev_right = 0, prev_start = 0;
    CountRepeat vertical_repeat = {0}, horizontal_repeat = {0};
    FpsCounter fps_counter;

    select_gamepad(0);
    reset_workload(mode);
    fps_counter_reset(&fps_counter, (unsigned)get_frame_counter());
    for (;;) {
        int up = gamepad_up() > 0;
        int down = gamepad_down() > 0;
        int dpad_left = gamepad_left() > 0;
        int dpad_right = gamepad_right() > 0;
        int left = gamepad_button_l() > 0;
        int right = gamepad_button_r() > 0;
        int start = gamepad_button_start() > 0;
        if (start && !prev_start) {
            run_tour();
            /* Resume the selected workload and wait for a new Start press. */
            reset_workload(mode);
            fps_counter_reset(&fps_counter, (unsigned)get_frame_counter());
            cpu_percent = 0;
            gpu_percent = 0;
            vertical_repeat.direction = 0;
            horizontal_repeat.direction = 0;
            prev_left = gamepad_button_l() > 0;
            prev_right = gamepad_button_r() > 0;
            prev_start = 1;
            continue;
        }
        if ((left && !prev_left) || (right && !prev_right)) {
            if (right && !prev_right)
                mode = mode == MODE_ALPHA ? MODE_BOIDS : mode + 1;
            else
                mode = mode == MODE_BOIDS ? MODE_ALPHA : mode - 1;
            reset_workload(mode);
            fps_counter_reset(&fps_counter, (unsigned)get_frame_counter());
            cpu_percent = 0;
            gpu_percent = 0;
            vertical_repeat.direction = 0;
            horizontal_repeat.direction = 0;
        }
        {
            unsigned frame = (unsigned)get_frame_counter();
            int vertical = count_repeat_steps(&vertical_repeat, up ? 1 : (down ? -1 : 0), frame);
            int horizontal = count_repeat_steps(&horizontal_repeat, dpad_right ? 1 : (dpad_left ? -1 : 0), frame);
            int step = mode == MODE_BOIDS ? 10 : (mode == MODE_BUNNIES ? 100 : 5);
            if (vertical != 0 || horizontal != 0) {
                int count = workload_count(mode) + vertical * step + horizontal;
                set_workload_count(mode, count);
            }
        }
        prev_left = left;
        prev_right = right;
        prev_start = start;
        benchmark_frame(mode, &cpu_percent, &gpu_percent, &fps_counter);
    }
}
