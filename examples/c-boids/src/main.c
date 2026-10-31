/* Cartridge entry point for the shared benchmark controller. */
#include <vircon.h>

#include "benchmark.h"

/* Start interactive mode or a deterministic profiling scenario. */
void main(void)
{
    run_benchmark();
}
