/* Runtime coverage for the software i32.div_u and i32.rem_u helper.
 * Volatile inputs prevent Clang from replacing the operations with constants. */
static volatile unsigned dividends[] = {
    11u,
    100u,
    0x80000000u,
    0xFFFFFFFFu,
};

static volatile unsigned divisors[] = {
    10u,
    10u,
    3u,
    10u,
};

static const unsigned expected_quotients[] = {
    1u,
    10u,
    715827882u,
    429496729u,
};

static const unsigned expected_remainders[] = {
    1u,
    0u,
    2u,
    5u,
};

/* Keep quotient and remainder as separate Wasm operations so the test covers
 * both frontend paths instead of allowing Clang to share one division. */
__attribute__((noinline)) static unsigned divide_unsigned(unsigned dividend, unsigned divisor) {
  return dividend / divisor;
}

__attribute__((noinline)) static unsigned remainder_unsigned(unsigned dividend, unsigned divisor) {
  return dividend % divisor;
}

int main(void) {
  unsigned index;

  for (index = 0; index < 4; ++index) {
    unsigned dividend = dividends[index];
    unsigned divisor = divisors[index];

    if (divide_unsigned(dividend, divisor) != expected_quotients[index])
      return (int)(index * 2u + 1u);
    if (remainder_unsigned(dividend, divisor) != expected_remainders[index])
      return (int)(index * 2u + 2u);
  }

  return 0;
}
