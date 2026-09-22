/* Verify the high-half sticky-bit identity used by the target lowering. */
#include <float.h>
#include <stdint.h>
#include <string.h>

static uint32_t float_bits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

/* Model the CIF/FADD sequence used for Wasm f32.convert_i32_u. */
static float convert_u32_like_target(uint32_t value)
{
    if (value < UINT32_C(0x80000000)) return (float)(int32_t)value;

    value = (value >> 1) | (value & 1);
    return (float)(int32_t)value + (float)(int32_t)value;
}

static int check(uint32_t value)
{
    return float_bits(convert_u32_like_target(value)) == float_bits((float)value);
}

int main(void)
{
    static const uint32_t bases[] = {
        UINT32_C(0x00000000), UINT32_C(0x00FFFF00), UINT32_C(0x7FFFFF00),
        UINT32_C(0x80000000), UINT32_C(0x80000100), UINT32_C(0x9ABCDE00),
        UINT32_C(0xFFFFFE00), UINT32_C(0xFFFFFF00)
    };
    size_t base_index;
    uint32_t offset;

    if (sizeof(float) != 4 || FLT_RADIX != 2) return 2;
    for (base_index = 0; base_index < sizeof(bases) / sizeof(bases[0]); ++base_index)
        for (offset = 0; offset < 256; ++offset)
            if (!check(bases[base_index] + offset)) return 1;
    return !check(UINT32_C(0xFFFFFFFF));
}
