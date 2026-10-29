/* Exercise Clang's actual i64.rotate Wasm instructions and both result words.
 * Returning zero means every runtime result matched an independent constant. */

static const unsigned counts[] = {0, 1, 31, 32, 33, 63, 64, 65, 127, 128};
static const unsigned long long left_results[] = {
    0x0123456789abcdefULL, 0x02468acf13579bdeULL, 0xc4d5e6f78091a2b3ULL,
    0x89abcdef01234567ULL, 0x13579bde02468acfULL, 0x8091a2b3c4d5e6f7ULL,
    0x0123456789abcdefULL, 0x02468acf13579bdeULL, 0x8091a2b3c4d5e6f7ULL,
    0x0123456789abcdefULL,
};
static const unsigned long long right_results[] = {
    0x0123456789abcdefULL, 0x8091a2b3c4d5e6f7ULL, 0x13579bde02468acfULL,
    0x89abcdef01234567ULL, 0xc4d5e6f78091a2b3ULL, 0x02468acf13579bdeULL,
    0x0123456789abcdefULL, 0x8091a2b3c4d5e6f7ULL, 0x02468acf13579bdeULL,
    0x0123456789abcdefULL,
};

/* The builtins preserve the intended operation through Clang's Wasm backend. */
int main(void) {
  unsigned index;
  volatile unsigned long long value = 0x0123456789abcdefULL;

  for (index = 0; index < sizeof(counts) / sizeof(counts[0]); ++index) {
    unsigned long long current = value;
    volatile unsigned long long count = counts[index];
    if (__builtin_rotateleft64(current, (unsigned)count) != left_results[index])
      return (int)(index * 2u + 1u);
    if (__builtin_rotateright64(current, (unsigned)count) != right_results[index])
      return (int)(index * 2u + 2u);
  }
  return 0;
}
