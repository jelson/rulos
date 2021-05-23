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

#include "core/rtc.h"

#include "core/rulos.h"

#include <stdbool.h>
#include <stdint.h>

void schedule_next_tick(rtc_t *rtc);

static void rtc_tick(void *data) {
  rtc_t *rtc = (rtc_t *)data;
  rtc->seconds_since_boot++;
  rtc->curr_second_start_us += 1000000;
  schedule_next_tick(rtc);
}

void schedule_next_tick(rtc_t *rtc) {
  schedule_absolute(rtc->curr_second_start_us + 1000000, rtc_tick, rtc);
}

void rtc_init(rtc_t *rtc) {
  memset(rtc, 0, sizeof(*rtc));
  rtc->curr_second_start_us = clock_time_us();
  schedule_next_tick(rtc);
}
  
void rtc_get_uptime(rtc_t *rtc, uint32_t *sec /* OUT */,
                    uint32_t *usec /* OUT */) {
  uint32_t us_since_last_tick = precise_clock_time_us() - rtc->curr_second_start_us;
  *sec = rtc->seconds_since_boot + us_since_last_tick / 1000000;
  *usec = us_since_last_tick % 1000000;
}

