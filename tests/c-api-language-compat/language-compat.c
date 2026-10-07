/* Verifies source-level conveniences supplied by vircon.h without hosted C
 * headers. Unlike the official word-addressed compiler, NULL is normal C/Wasm
 * zero because pointers are byte offsets in the linear-memory frontend. */

#include <vircon.h>

static bool ready = true;

int main(void)
{
    bool disabled = false;
    void *nothing = NULL;

    if (!ready || disabled || nothing != NULL)
        return 1;
    if (INT_MIN >= 0 || INT_MAX <= 0 || pi <= 3.0f)
        return 2;
    if (blending_alpha != 0x20 || blending_add != 0x21 || blending_subtract != 0x22)
        return 3;
    if (bios_character_width != 10 || bios_character_height != 20)
        return 4;
    return 0;
}
