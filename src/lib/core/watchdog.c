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

#include "core/watchdog.h"

#include "core/rulos.h"

static void watchdog_tick(void *data) {
  watchdog_t *watchdog = (watchdog_t *)data;
  if (later_than(clock_time_us(), watchdog->next_deadline)) {
    LOG("WATCHDOG: %d-sec timeout expired, resetting", watchdog->timeout_sec);
    hal_reset();
  }

  schedule_us(500000, watchdog_tick, watchdog);
}

void watchdog_keepalive(watchdog_t *watchdog) {
  watchdog->next_deadline = clock_time_us() + watchdog->timeout_sec * 1000000;
}

void watchdog_init(watchdog_t *watchdog, uint32_t timeout_sec) {
  memset(watchdog, 0, sizeof(*watchdog));
  watchdog->timeout_sec = timeout_sec;
  watchdog_keepalive(watchdog);
  schedule_us(0, watchdog_tick, watchdog);
}
