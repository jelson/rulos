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

// HAL private to ARM chips -- i.e., the interface that all ARM chips
// expose.

#include <inttypes.h>

#include "core/hal.h"

uint32_t arm_hal_get_clock_rate();

void arm_hal_start_clock_us(uint32_t ticks_per_interrupt, Handler handler,
                            void *data);
