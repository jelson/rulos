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
typedef uint32_t Time;

// Returns true if `a` is later than `b` using rollover math
bool later_than(Time a, Time b);

// Returns true if `a` is later than or equal to `b` using rollover math
bool later_than_or_eq(Time a, Time b);

// Returns the delta from a to b, using rollover logic. If later_than(a, b),
// will return a negative number, otherwise returns a positive number.
int32_t time_delta(Time a, Time b);

// Convert seconds and milliseconds to microseconds
Time time_sec(uint16_t seconds);
Time time_msec(uint32_t msec);
