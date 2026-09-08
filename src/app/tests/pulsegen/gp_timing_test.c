#include "gp_timing.h"

#include <assert.h>
#include <stdio.h>

static void test_edges(void) {
  uint32_t psc, arr, rise, fall;
  uint64_t tick;
  assert(gp_select_timebase(1000000000ULL, &psc, &arr, &tick));
  assert((arr + 1) * tick == 1000000000ULL);
  const unsigned delays_us[] = {0, 970, 974, 980, 984, 990};
  for (unsigned i = 0; i < sizeof(delays_us) / sizeof(delays_us[0]); i++) {
    gp_select_edges(delays_us[i] * 1000000ULL, 10000000ULL, arr, tick, &rise, &fall);
    assert(rise * tick == delays_us[i] * 1000000ULL);
    assert((fall - rise) * tick == 10000000ULL);
    assert(fall <= arr + 1);
    if (delays_us[i] == 990) {
      assert(fall == arr + 1);
    }
  }

  // Four SYNC windows may collectively cover the whole period: gating needs no shared idle gap.
  const unsigned wide_delays_us[] = {0, 100, 200, 250};
  for (unsigned i = 0; i < 4; i++) {
    gp_select_edges(wide_delays_us[i] * 1000000ULL, 750000000ULL, arr, tick, &rise, &fall);
    assert(rise * tick == wide_delays_us[i] * 1000000ULL);
    assert((fall - rise) * tick == 750000000ULL);
  }
}

static void test_timebase_limits(void) {
  uint32_t psc, arr, rise, fall;
  uint64_t tick;
  assert(gp_select_timebase(1000000000000ULL, &psc, &arr, &tick));
  assert((arr + 1) * tick == 1000000000000ULL);
  assert(gp_select_timebase(65536ULL * GP_TICK_PS, &psc, &arr, &tick));
  assert((arr + 1) * tick == 65536ULL * GP_TICK_PS);
  assert(arr < 65535);
  assert(gp_select_timebase(GP_MAX_PERIOD_PS, &psc, &arr, &tick));
  assert(psc == 65535 && arr == 65534);
  assert(GP_MAX_PERIOD_PS - (arr + 1) * tick == tick);
  gp_select_edges(GP_MAX_PERIOD_PS / 4, 3 * GP_MAX_PERIOD_PS / 4, arr, tick, &rise, &fall);
  assert(fall == arr + 1 && fall <= 65535);
  assert(fall > rise);
  assert(!gp_select_timebase(GP_MAX_PERIOD_PS + 1, &psc, &arr, &tick));
  assert(!gp_select_timebase(UINT64_MAX, &psc, &arr, &tick));
  assert(!gp_select_timebase(0, &psc, &arr, &tick));

  // Sub-tick widths and top-of-range rounding still yield one high tick and one low tick.
  gp_select_edges(UINT64_MAX, 1, arr, tick, &rise, &fall);
  assert(fall - rise == 1 && fall == arr + 1);
  gp_select_edges(0, UINT64_MAX, arr, tick, &rise, &fall);
  assert(rise == 0 && fall == arr);
  assert(gp_arm_guard_ticks(16000000, 16000) == 1000);
  assert(gp_arm_guard_ticks(16000000, 524288000) == 1);
}

static void test_burst(uint32_t pulses, bool deferred) {
  gp_burst_t burst = {.remaining = pulses, .arm_pending = deferred};
  bool preload_active = !deferred;
  uint32_t observed = 0;
  // Each loop models a hardware update followed by its ISR. Compare preloads affect only the
  // following cycle, so even a single pulse gets its full [0,T) opportunity before gating closes.
  for (uint32_t update = 0; update < pulses + 4; update++) {
    bool active = preload_active;
    switch (gp_burst_on_update(&burst)) {
      case GP_BURST_STAGE_ACTIVE:
        assert(!active);
        preload_active = true;
        burst.arm_pending = false;
        break;
      case GP_BURST_STAGE_INACTIVE:
        assert(active);
        preload_active = false;
        break;
      default:
        break;
    }
    // The ISR cannot truncate this cycle, including its zero-delay or period-ending pulse.
    if (active) {
      observed++;
    }
  }
  assert(observed == pulses);
  assert(!preload_active && burst.remaining == 0);
}

int main(void) {
  test_edges();
  test_timebase_limits();
  for (uint32_t count = 1; count <= 65534; count *= 2) {
    test_burst(count, false);
    test_burst(count, true);
  }
  test_burst(65534, false);
  test_burst(65534, true);
  puts("GP timing and burst-preload tests passed");
  return 0;
}
