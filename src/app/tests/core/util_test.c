#include "core/util.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void log_assert(const char *file, unsigned long line) {
  fprintf(stderr, "%s:%lu: assertion failed\n", file, line);
  abort();
}

int main(void) {
  uint32_t root = 0;
  for (uint32_t v = 0; v < 200000; v++) {
    if ((root + 1) * (root + 1) <= v) {
      root++;
    }
    assert(isqrt(v) == root);
  }
  for (uint32_t n = 1; n <= 65535; n += 997) {
    assert(isqrt(n * n) == n);
    assert(isqrt(n * n - 1) == n - 1);
  }
  assert(isqrt(65535u * 65535u) == 65535);
  assert(isqrt(UINT32_MAX) == 65535);
  puts("util: isqrt exact on perfect squares and bounded at the 32-bit limit");
}
