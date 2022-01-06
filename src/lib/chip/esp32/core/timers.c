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

#include <stdbool.h>
#include <stdint.h>

#include "core/hal.h"
#include "core/hardware.h"
#include "core/rulos.h"
#include "driver/timer.h"
#include "soc/soc.h"

typedef struct {
  // rulos data
  Handler cb;
  void *cb_data;

  // pointers to underlying hardware registers
  timer_group_t group;
  timer_idx_t index;
  uint32_t intr_source;
} esp32_timer_t;

#define NUM_TIMERS 4

static esp32_timer_t esp32_timer[NUM_TIMERS] = {
    {
        .group = TIMER_GROUP_0,
        .index = TIMER_0,
        .intr_source = ETS_TG0_T0_EDGE_INTR_SOURCE,
    },
    {
        .group = TIMER_GROUP_0,
        .index = TIMER_1,
        .intr_source = ETS_TG0_T1_EDGE_INTR_SOURCE,
    },
    {
        .group = TIMER_GROUP_1,
        .index = TIMER_0,
        .intr_source = ETS_TG1_T0_EDGE_INTR_SOURCE,
    },
    {
        .group = TIMER_GROUP_1,
        .index = TIMER_1,
        .intr_source = ETS_TG1_T1_EDGE_INTR_SOURCE,
    },
};

static esp32_timer_t *get_timer(uint8_t timer_id) {
  assert(timer_id >= 0 && timer_id < NUM_TIMERS);
  return &esp32_timer[timer_id];
}

static bool IRAM_ATTR timer_isr(void *arg) {
  esp32_timer_t *const eu = (esp32_timer_t *)arg;
  eu->cb(eu->cb_data);
  return false;
}

uint32_t hal_start_clock_us(uint32_t us, Handler handler, void *data,
                            uint8_t timer_id) {
  esp32_timer_t *const eu = get_timer(timer_id);

  // store RULOS-specific data
  eu->cb = handler;
  eu->cb_data = data;

  // set up timer config
  timer_config_t config = {
      .alarm_en = TIMER_ALARM_EN,
      .counter_en = TIMER_PAUSE,
      .counter_dir = TIMER_COUNT_UP,
      .auto_reload = TIMER_AUTORELOAD_EN,
      .divider = 2,  // minimum allowed divider
  };
  timer_init(eu->group, eu->index, &config);

  // initialize timer to 0
  timer_set_counter_value(eu->group, eu->index, 0);

  // compute the period in counter ticks, set and enable the alarm
  uint64_t alarm_value = (uint64_t)getApbFrequency() * (uint64_t)us;
  alarm_value /= 1000000;
  alarm_value /= config.divider;
  timer_set_alarm_value(eu->group, eu->index, alarm_value);

  // attach and enable interrupts
  timer_enable_intr(eu->group, eu->index);
  timer_isr_callback_add(eu->group, eu->index, timer_isr, eu, 0);

  // enable the timer
  timer_start(eu->group, eu->index);

  LOG("starting timer %d with a reload value of %d", timer_id,
      (uint32_t)alarm_value);

  // return the exact timer period, in microseconds, accounting for
  // any rouding that might have happened
  return (alarm_value * config.divider * 1000000) / getApbFrequency();
}

uint16_t hal_elapsed_milliintervals() {
  return 0;
}

bool hal_clock_interrupt_is_pending() {
  return false;
}
