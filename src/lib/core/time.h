/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

#pragma once

#include <stdint.h>
#include "core/util.h"

// Definition of time for RulOS, in units of usec.
typedef int32_t Time;

// Returns true if b is later than a using rollover math, assuming 32-bit
// signed time values.
static inline r_bool later_than(Time a, Time b) {
	// the subtraction will roll over too
	return a - b > 0;

	// this took forever to puzzle out and was originally a
	// complicated set of conditionals
}

// Returns true if b is later than or equal to a using rollover math,
// assuming 32-bit signed time values.
static inline r_bool later_than_or_eq(Time a, Time b) {
	// the subtraction will roll over too
	return a - b >= 0;
}
