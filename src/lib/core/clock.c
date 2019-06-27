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

#include <stdint.h>
#include <stdlib.h>

#include "core/clock.h"
#include "core/logging.h"

#ifdef TIMING_DEBUG
#include "core/hardware.h"
#endif

static Clock g_clock;

Clock::Clock() {
  // Initialize the clock to 20 seconds before rollover time so that
  // rollover bugs happen quickly during testing
  interrupt_driven_jiffy_clock_+= Interval::usec(INT32_MAX - 20000000);
}

void Clock::handler(void *data) {
  // NB, this runs in interrupt context.
  g_clock.interrupt_driven_jiffy_clock_ += g_clock.jiffy_interval_;
  //  run_scheduler_now = TRUE; ????
}

void Clock::Start(const Interval& jiffy_interval, const uint8_t timer_id) {
  g_clock.jiffy_interval_ =
    hal_start_clock_us(jiffy_interval, &Clock::handler, NULL, timer_id);
}

Time Clock::GetTime() {
  Time retval;
  rulos_irq_state_t old_interrupts = hal_start_atomic();
  retval = g_clock.interrupt_driven_jiffy_clock_;
  hal_end_atomic(old_interrupts);
  return retval;
}

// Get the time including a query to the underlying hardware.
Time Clock::GetPreciseTime() {
  rulos_irq_state_t old_interrupts = hal_start_atomic();
  uint16_t milliintervals = hal_elapsed_milliintervals();
  Time retval = g_clock.interrupt_driven_jiffy_clock_;
  hal_end_atomic(old_interrupts);

  retval += Interval::usec(
			   (g_clock.jiffy_interval_.to_usec() * milliintervals) / 1000);
  return retval;
}


