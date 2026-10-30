/* Defined functions for testing i64 parameters, returns and nested calls. */
typedef unsigned long long u64;

/* Combine mixed-width parameters without specializing their values. */
__attribute__((noinline)) u64 combine(int a, u64 x, int b, u64 y, int c) {
  return x + y + (unsigned)a + (unsigned)b + (unsigned)c;
}

/* Return an i64 value which another function immediately consumes. */
__attribute__((noinline)) u64 increment(u64 value) { return value + 1; }

/* Preserve live caller arguments across nested invocations. */
__attribute__((noinline)) u64 nested(u64 value, u64 other) {
  u64 saved = value;
  return combine(1, increment(saved), 2, other, 3) + saved;
}
