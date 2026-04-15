/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdint.h>

#include "core/logging.h"  // for RULOS's assert() (not <assert.h>)

// Helper used by each family's hal_elapsed_us_in_tick implementation.
// Computes a (k_num, k_denom) split such that:
//   k_num × k_denom == us_per_period  (exact; no scale drift)
//   k_num × elapsed_max < 2^32         (overflow-safe multiply)
// The hot path then computes:
//   return (k_num × elapsed_ticks) / (elapsed_max / k_denom)
// which is algebraically us_per_period × elapsed_ticks / elapsed_max but
// without the overflow that would otherwise blow out u32 for tick
// intervals larger than ~65ms on AVR or ~5ms on STM32.
//
// We minimize precision loss by making k_num as large as possible (as
// close to 2^32 / elapsed_max as possible), leaving k_denom as small as
// possible. All precision loss is on the denominator: elapsed_max is
// pre-divided by k_denom, so the hot path loses log2(k_denom) bits of
// denominator resolution.

typedef struct {
  uint32_t k_num;
  uint32_t k_denom;
} clock_split_t;

static inline clock_split_t clock_split(uint32_t us_per_period,
                                        uint32_t elapsed_max) {
  // Pick the smallest k_denom such that us_per_period / k_denom times
  // elapsed_max fits in u32. Uses u64 for this one init-time calc to
  // dodge the very overflow we're engineering around in the hot path.
  uint64_t product = (uint64_t)us_per_period * elapsed_max;
  uint32_t k_denom = (uint32_t)((product + UINT32_MAX) >> 32);
  if (k_denom == 0) {
    k_denom = 1;
  }

  // Grow k_denom until it divides us_per_period cleanly, so the product
  // k_num × k_denom recovers us_per_period exactly with no scale drift.
  while (us_per_period % k_denom != 0) {
    k_denom++;
    // Sanity cap: any sensible tick interval has a small divisor ≥
    // k_denom_min. This trips only for pathological intervals (e.g.
    // a prime number of microseconds).
    assert(k_denom <= 1000);
  }

  return (clock_split_t){
      .k_num = us_per_period / k_denom,
      .k_denom = k_denom,
  };
}
