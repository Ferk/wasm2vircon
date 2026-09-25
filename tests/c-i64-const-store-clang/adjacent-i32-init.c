/*
 * At -O2, Clang coalesces these adjacent assignments into the narrow
 * i64.store/i64.const form accepted by VirconWasm v1.6. The imported call
 * receives the static object's address, so the initialization is observable
 * and remains in the generated Wasm.
 */
struct Pair {
    int low;
    int high;
};

static struct Pair pair;

extern void vircon_set_background_color(int color);

int main(void)
{
    pair.low = 0x55667788;
    pair.high = 0x11223344;
    vircon_set_background_color((int)(unsigned long)&pair);
    return 0;
}
