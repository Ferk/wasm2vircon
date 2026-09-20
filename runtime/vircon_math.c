#include "vircon.h"
#include "vircon_platform.h"

/* The currently supported callers use finite values within the documented
 * target domains. Keep these as normal C definitions, rather than exposing
 * C-library names through the platform-import ABI. */
float sinf(float x) { return vircon_cpu_sin(x); }
float cosf(float x) { return vircon_cpu_sin(x + 1.57079632679f); }
float tanf(float x) { return sinf(x) / cosf(x); }
float acosf(float x) { return vircon_cpu_acos(x); }
float asinf(float x) { return 1.57079632679f - acosf(x); }
float expf(float x) { return vircon_cpu_pow(2.71828182846f, x); }
float logf(float x) { return vircon_cpu_log(x); }
float powf(float x, float y) { return vircon_cpu_pow(x, y); }
