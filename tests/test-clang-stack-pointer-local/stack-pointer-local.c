/* A local aggregate forces Clang's canonical wasm-ld stack-pointer ABI shape. */
struct Pair {
    int first;
    int second;
};

/* Keep the aggregate address observable across a direct defined call. */
__attribute__((noinline)) int read_pair(volatile struct Pair *pair)
{
    return pair->first + pair->second;
}

int __original_main(void)
{
    volatile struct Pair pair;
    pair.first = 17;
    pair.second = 25;
    return read_pair(&pair);
}
