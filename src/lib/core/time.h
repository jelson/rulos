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

#include "core/util.h"

// Definition of time for RulOS, in units of usec.
typedef int32_t Time;

// Returns true if b is later than a using rollover math, assuming 32-bit
// signed time values.
static inline bool later_than(Time a, Time b) {
  // the subtraction will roll over too
  return a - b > 0;

  // this took forever to puzzle out and was originally a
  // complicated set of conditionals
}

// Returns true if b is later than or equal to a using rollover math,
// assuming 32-bit signed time values.
static inline bool later_than_or_eq(Time a, Time b) {
  // the subtraction will roll over too
  return a - b >= 0;
}

static inline Time time_sec(uint16_t seconds) {
  return ((Time)1000000) * seconds;
}

static inline Time time_msec(uint32_t msec) { return ((Time)1000) * msec; }
