// Freestanding C++ cartridge entry for the shared benchmark controller.
#include "benchmark.h"

// Keep the Wasm export stable for the Vircon32 build.
extern "C" void vircon_main()
{
    run_benchmark();
}
