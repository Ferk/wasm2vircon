/* Frontend/runtime coverage for signed byte loads and unaligned 16-bit
 * accesses in packed Wasm linear memory. Each object is word-aligned while
 * its halfword begins in a different byte lane. */

#include <stdint.h>

typedef struct __attribute__((packed, aligned(4))) {
    volatile uint16_t value;
    volatile uint8_t after[2];
} Lane0;

typedef struct __attribute__((packed, aligned(4))) {
    volatile uint8_t before[1];
    volatile uint16_t value;
    volatile uint8_t after[1];
} Lane1;

typedef struct __attribute__((packed, aligned(4))) {
    volatile uint8_t before[2];
    volatile uint16_t value;
} Lane2;

typedef struct __attribute__((packed, aligned(4))) {
    volatile uint8_t before[3];
    volatile uint16_t value;
    volatile uint8_t after[1];
} Lane3;

static Lane0 lane0 = {0, {0x31, 0x32}};
static Lane1 lane1 = {{0x41}, 0, {0x42}};
static Lane2 lane2 = {{0x51, 0x52}, 0};
static Lane3 lane3 = {{0x61, 0x62, 0x63}, 0, {0x64}};
static volatile int8_t signed_byte = -128;
static volatile int16_t signed_half = -32768;

int main(void)
{
    lane0.value = UINT16_C(0xa1b2);
    lane1.value = UINT16_C(0xc3d4);
    lane2.value = UINT16_C(0xe5f6);
    lane3.value = UINT16_C(0x8798);

    if (lane0.value != UINT16_C(0xa1b2) || lane0.after[0] != 0x31 || lane0.after[1] != 0x32)
        return 1;
    if (lane1.before[0] != 0x41 || lane1.value != UINT16_C(0xc3d4) || lane1.after[0] != 0x42)
        return 2;
    if (lane2.before[0] != 0x51 || lane2.before[1] != 0x52 || lane2.value != UINT16_C(0xe5f6))
        return 3;
    if (lane3.before[0] != 0x61 || lane3.before[1] != 0x62 || lane3.before[2] != 0x63 ||
        lane3.value != UINT16_C(0x8798) || lane3.after[0] != 0x64)
        return 4;
    if (signed_byte != -128 || signed_half != -32768)
        return 5;
    return 0;
}
