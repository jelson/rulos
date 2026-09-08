#pragma once

#include <stdbool.h>
#include <stdint.h>

// Reserve one compare value above ARR so a pulse may end exactly at the period boundary, even
// on the 16-bit timers. Keep the nominal period range; its uppermost value rounds down one tick.
#define GP_TICK_PS       8000ULL
#define GP_PSC_MAX       0xFFFFU
#define GP_COUNTER_MAX   0xFFFFU
#define GP_MAX_PERIOD_PS ((uint64_t)65536 * 65536 * GP_TICK_PS)

static inline bool gp_select_timebase(uint64_t period_ps, uint32_t *out_psc, uint32_t *out_arr,
                                      uint64_t *out_tick_ps) {
  if (period_ps > GP_MAX_PERIOD_PS) {
    return false;
  }
  const uint64_t target = (period_ps + GP_TICK_PS / 2) / GP_TICK_PS;
  const uint64_t div_min = (target + GP_COUNTER_MAX) / (GP_COUNTER_MAX + 1);
  uint64_t best_div = 0, best_ticks = 0, best_err = UINT64_MAX;
  for (uint64_t div = div_min < 1 ? 1 : div_min; div <= GP_PSC_MAX + 1; div++) {
    uint64_t ticks = (target + div / 2) / div;
    if (ticks > GP_COUNTER_MAX) {
      ticks = GP_COUNTER_MAX;
    }
    if (ticks < 2) {
      break;
    }
    uint64_t actual = ticks * div;
    uint64_t err = actual > target ? actual - target : target - actual;
    if (err < best_err) {
      best_err = err;
      best_div = div;
      best_ticks = ticks;
      if (err == 0) {
        break;
      }
    }
  }
  if (best_div == 0) {
    return false;
  }
  *out_psc = (uint32_t)(best_div - 1);
  *out_arr = (uint32_t)(best_ticks - 1);
  *out_tick_ps = GP_TICK_PS * best_div;
  return true;
}

// Combined PWM2 AND PWM1 is high in [rise, fall). Quantization must retain both an active and
// an inactive tick, and may put fall at ARR+1: the sibling then stays high until the rollover.
static inline void gp_select_edges(uint64_t delay_ps, uint64_t width_ps, uint32_t arr,
                                   uint64_t tick_ps, uint32_t *rise, uint32_t *fall) {
  const uint32_t period = arr + 1;
  uint64_t width = width_ps / tick_ps;
  if (width < 1) {
    width = 1;
  } else if (width >= period) {
    width = period - 1;
  }
  uint64_t delay = delay_ps / tick_ps;
  if (delay > period - width) {
    delay = period - width;
  }
  *rise = (uint32_t)delay;
  *fall = (uint32_t)(delay + width);
}

// The current prescaled tick may be almost over. Only the ticks strictly after CNT provide
// guaranteed time for an atomic shadow-register batch before the next update.
static inline uint32_t gp_arm_guard_ticks(uint64_t guard_ps, uint64_t tick_ps) {
  return (uint32_t)((guard_ps + tick_ps - 1) / tick_ps);
}

typedef struct {
  uint32_t remaining;
  bool arm_pending;
} gp_burst_t;

typedef enum {
  GP_BURST_NO_CHANGE,
  GP_BURST_STAGE_ACTIVE,
  GP_BURST_STAGE_INACTIVE,
} gp_burst_action_t;

// Called after hardware has latched the preceding update's preloads. A pending arm has not
// emitted a pulse yet; otherwise the final pulse's start must schedule an inactive next cycle.
static inline gp_burst_action_t gp_burst_on_update(volatile gp_burst_t *state) {
  if (state->arm_pending) {
    return GP_BURST_STAGE_ACTIVE;
  }
  if (state->remaining != 0 && --state->remaining == 0) {
    return GP_BURST_STAGE_INACTIVE;
  }
  return GP_BURST_NO_CHANGE;
}
