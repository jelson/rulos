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

#include "core/wallclock.h"

#include <stdbool.h>
#include <stdint.h>

#include "core/rulos.h"

void schedule_next_tick(wallclock_t *wallclock);

static void wallclock_tick(void *data) {
  wallclock_t *wallclock = (wallclock_t *)data;
  rulos_irq_state_t old_intr;
  old_intr = hal_start_atomic();
  wallclock->seconds_since_boot++;
  wallclock->curr_second_start_us += 1000000;
  hal_end_atomic(old_intr);
  schedule_next_tick(wallclock);
}

void schedule_next_tick(wallclock_t *wallclock) {
  schedule_absolute(wallclock->curr_second_start_us + 1000000, wallclock_tick,
                    wallclock);
}

void wallclock_init(wallclock_t *wallclock) {
  memset(wallclock, 0, sizeof(*wallclock));
  wallclock->curr_second_start_us = clock_time_us();
  schedule_next_tick(wallclock);
}

void wallclock_get_uptime(wallclock_t *wallclock, uint32_t *sec /* OUT */,
                          uint32_t *usec /* OUT */) {
  // This should be correct even there's a callback pending. In that
  // case, us_since_last_tick might be greater than 1,000,000.
  uint32_t us_since_last_tick =
      precise_clock_time_us() - wallclock->curr_second_start_us;
  *sec = wallclock->seconds_since_boot + us_since_last_tick / 1000000;
  *usec = us_since_last_tick % 1000000;
}

uint64_t wallclock_get_uptime_usec(wallclock_t *wallclock) {
  uint32_t sec, usec;
  wallclock_get_uptime(wallclock, &sec, &usec);
  return ((uint64_t) sec) * 1000000 + usec;
}
