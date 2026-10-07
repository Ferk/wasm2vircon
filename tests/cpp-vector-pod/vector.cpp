/*
 * Minimal freestanding C++ container probe.
 *
 * This uses the official libc++ <vector> supplied by a Wasm C++ SDK and
 * measures the retained library/ABI requirements of a POD vector.
 */
#define VIRCON_IMPLEMENTATION
#include <vircon.h>
#include <vector>

/* Also checks the no-hosted-exit destructor registration path retained for a
 * global vector. Its all-zero static representation is valid before use. */
static std::vector<int> global_values;

extern "C" void vircon_main() {
  global_values.reserve(4);
  global_values.push_back(10);
  global_values.push_back(20);
  int *single = new int(9);
  volatile int result = global_values[0] + global_values[1] + (int)global_values.size() + *single;
  delete single;
  if (result != 41)
    vircon__cpu_halt();
}
