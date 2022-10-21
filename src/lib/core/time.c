#include "core/time.h"

#include <stdint.h>

#define HALF_RANGE (UINT32_MAX / 2)

// Important note: rollover/overflow math as used by the code below is only
// defined for *unsigned* types in C. I painfully discovered this when the
// 14-year-old scheduler broke after upgrading from gcc 10 to 11. Time used to
// be a int32_t, and since overflow is undefined, the original later_than(),
// return a - b > 0, was optimized to "return a > b". This is a valid
// transformation because rollover values are not defined in C for signed
// values.

// returns true if a is later than b
bool later_than(Time a, Time b) {
  return a - b < HALF_RANGE;
}

bool later_than_or_eq(Time a, Time b) {
  if (a == b) {
    return true;
  } else {
    return later_than(a, b);
  }
}

// returns time in usec from a-->b.
//
// This seems like it can be implemented with
//
// return (int32_t) b - a
//
// ...but I'm doing it a little more explicitly in case casting from uint32_t to
// int32_t is also undefined.
int32_t time_delta(Time a, Time b) {
  Time delta = b - a;

  if (delta > HALF_RANGE) {
    return -(UINT32_MAX - delta + 1);
  } else {
    return delta;
  }
}

Time time_sec(uint16_t seconds) {
  return ((Time)1000000) * seconds;
}

Time time_msec(uint32_t msec) {
  return ((Time)1000) * msec;
}
