// Cartridge entry and frame presentation for the freestanding C++ boids demo.
#include <vircon.h>

#include "boids.hpp"

// The application owns the frame loop; boids.cpp owns only benchmark state,
// workload changes, flocking simulation and flock rendering. The explicit C
// linkage keeps this freestanding C++ entry name stable in the Wasm export.
extern "C" int vircon_main()
{
    int previous_up_pressed = 0;
    int previous_down_pressed = 0;
    int cpu_percent = 0;

    boids::initialize();
    for (;;) {
        const int start_frame = get_frame_counter();
        const int start_cycles = get_cycle_counter();

        select_gamepad(0);
        const int up_pressed = gamepad_up() > 0;
        const int down_pressed = gamepad_down() > 0;
        if (up_pressed && !previous_up_pressed)
            boids::increase();
        else if (down_pressed && !previous_down_pressed)
            boids::decrease();
        previous_up_pressed = up_pressed;
        previous_down_pressed = down_pressed;

        const int update_start_frame = get_frame_counter();
        boids::update();
        const int update_frames = get_frame_counter() - update_start_frame + 1;
        const float frames_per_update = static_cast<float>(frames_per_second) /
            static_cast<float>(update_frames);

        clear_screen(color_black);
        boids::draw();
        print_at(10, 10, "Boids:");
        print_uint_at(80, 10, static_cast<unsigned>(boids::count()));
        print_at(120, 10, "(up/down +/-10)");
        print_at(10, 30, "FPS:");
        print_fixed_2_at(60, 30, frames_per_update);
        print_at(10, 50, "CPU:");
        print_uint_at(60, 50, static_cast<unsigned>(cpu_percent));
        print_at(90, 50, "%");

        // Display the previous frame's measurement while measuring this one.
        cpu_percent =
            ((get_frame_counter() - start_frame) * 250000 +
             get_cycle_counter() - start_cycles) * 100 / 250000;
        end_frame();
    }
}
