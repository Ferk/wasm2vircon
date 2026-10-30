/* Exercise actual Clang-generated i64 calls and carry across both words. */
typedef unsigned long long u64;
static volatile u64 first = 0x123456789abcdef0ULL;
static volatile u64 second = 0x1020304050607080ULL;

u64 combine(int a, u64 x, int b, u64 y, int c);
u64 nested(u64 value, u64 other);
typedef u64 (*combine_function)(int, u64, int, u64, int);
static combine_function volatile selected = combine;

/* Return zero when the generated code preserves both result words. */
int main(void) {
  if (combine(1, first, 2, second, 3) != 0x225486b8eb1d4f76ULL)
    return 1;
  if (nested(first, second) != 0x3488dd3185da2e67ULL)
    return 2;
  if (selected(1, first, 2, second, 3) != 0x225486b8eb1d4f76ULL)
    return 3;
  return 0;
}
