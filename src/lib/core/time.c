#include "core/time.h"

#include <stdint.h>

#define HALF_RANGE (UINT32_MAX / 2)

// returns true if a is later than b
bool later_than(Time a, Time b) {
  if (a > b) {
    return (a - b) < HALF_RANGE;
  } else {
    return (b - a) > HALF_RANGE;
  }
}

bool later_than_or_eq(Time a, Time b) {
  if (a == b) {
    return true;
  } else {
    return later_than(a, b);
  }
}

// returns time in usec from a-->b.
int32_t time_delta(Time a, Time b) {
  uint32_t delta;
  if (a > b) {
    delta = a - b;
  } else {
    delta = b - a;
  }

  if (later_than(b, a)) {
    if (delta < HALF_RANGE)
      return delta;
    else {
      return UINT32_MAX - delta + 1;
    }
  } else {
    if (delta < HALF_RANGE) {
      return -delta;
    } else {
      return delta - UINT32_MAX - 1;
    }
  }
}

Time time_sec(uint16_t seconds) {
  return ((Time)1000000) * seconds;
}

Time time_msec(uint32_t msec) {
  return ((Time)1000) * msec;
}
