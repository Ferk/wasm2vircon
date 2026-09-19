#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Reference checks for the byte-to-word ABI used by the v1 lowering tests. */
static uint8_t load8(const uint32_t *words, uint32_t pointer)
{
    return (uint8_t)(words[pointer >> 2] >> ((pointer & 3u) * 8u));
}

static void store8(uint32_t *words, uint32_t pointer, uint8_t value)
{
    uint32_t shift = (pointer & 3u) * 8u;
    uint32_t mask = UINT32_C(0xff) << shift;
    uint32_t *word = &words[pointer >> 2];
    *word = (*word & ~mask) | ((uint32_t)value << shift);
}

static uint32_t load32(const uint32_t *words, uint32_t pointer)
{
    uint32_t value = 0;
    for (uint32_t byte = 0; byte < 4; ++byte)
        value |= (uint32_t)load8(words, pointer + byte) << (byte * 8u);
    return value;
}

static void store32(uint32_t *words, uint32_t pointer, uint32_t value)
{
    for (uint32_t byte = 0; byte < 4; ++byte)
        store8(words, pointer + byte, (uint8_t)(value >> (byte * 8u)));
}

int main(void)
{
    uint32_t words[3] = {UINT32_C(0x44332211), UINT32_C(0x88776655), 0};
    const uint8_t expected[] = {0x11, 0x22, 0x33, 0x44};
    for (uint32_t lane = 0; lane < 4; ++lane)
        if (load8(words, lane) != expected[lane]) return 1;
    store8(words, 2, 0xaa);
    if (words[0] != UINT32_C(0x44aa2211)) return 2;
    for (uint32_t offset = 0; offset < 4; ++offset) {
        uint32_t copy[3];
        memcpy(copy, words, sizeof(copy));
        store32(copy, offset, UINT32_C(0x11223344));
        if (load32(copy, offset) != UINT32_C(0x11223344)) return 3 + (int)offset;
    }
    return 0;
}
