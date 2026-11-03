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

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include "core/hal.h"
#include "core/hardware.h"
#include "core/rulos.h"
#include "driver/gptimer.h"
#include "esp_attr.h"
#include "esp_private/gptimer.h"
#include "hal/timer_ll.h"
#include "soc/soc_caps.h"

// One count per microsecond, so the alarm period and the elapsed-time
// readback are both in the scheduler's units with no conversion.
#define TIMER_RESOLUTION_HZ 1000000

typedef struct {
  // rulos data
  clock_handler_t cb;
  void *cb_data;
  uint32_t us_per_period;

  gptimer_handle_t timer;

  // Where the timer lives in the timer-group peripheral, for reading the
  // alarm interrupt status directly.
  timg_dev_t *hw;
  int index;
} esp32_timer_t;

#define NUM_TIMERS (SOC_TIMER_GROUPS * SOC_TIMER_GROUP_TIMERS_PER_GROUP)

static esp32_timer_t esp32_timer[NUM_TIMERS];

static esp32_timer_t *get_timer(uint8_t timer_id) {
  assert(timer_id >= 0 && timer_id < NUM_TIMERS);
  return &esp32_timer[timer_id];
}

static bool IRAM_ATTR timer_isr(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata,
                                void *arg) {
  esp32_timer_t *const eu = (esp32_timer_t *)arg;
  hal_start_atomic();
  if (eu->cb) {
    eu->cb(eu->cb_data);
  }
  hal_end_atomic(0);
  // no higher-priority FreeRTOS task was woken
  return false;
}

// The gptimer driver hands out an opaque handle, but checking whether an
// alarm interrupt is pending needs the timer's slot in the timer-group
// register block. The group is available from the driver; the slot within
// it is found by parking a sentinel in the counter and seeing which slot
// reports it.
static void locate_in_timer_group(esp32_timer_t *eu) {
  int group;
  ESP_ERROR_CHECK(gptimer_get_group_id(eu->timer, &group));
  eu->hw = TIMER_LL_GET_HW(group);

  const uint64_t sentinel = 0x5A5A5A5A5A5Aull;
  ESP_ERROR_CHECK(gptimer_set_raw_count(eu->timer, sentinel));
  for (eu->index = 0; eu->index < SOC_TIMER_GROUP_TIMERS_PER_GROUP; eu->index++) {
    timer_ll_trigger_soft_capture(eu->hw, eu->index);
    if (timer_ll_get_counter_value(eu->hw, eu->index) == sentinel) {
      break;
    }
  }
  assert(eu->index < SOC_TIMER_GROUP_TIMERS_PER_GROUP);
  ESP_ERROR_CHECK(gptimer_set_raw_count(eu->timer, 0));
}

uint32_t hal_start_clock_us(uint32_t us, clock_handler_t handler, void *data, uint8_t timer_id) {
  esp32_timer_t *const eu = get_timer(timer_id);

  // store RULOS-specific data
  eu->cb = handler;
  eu->cb_data = data;
  eu->us_per_period = us;

  gptimer_config_t config = {};
  config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
  config.direction = GPTIMER_COUNT_UP;
  config.resolution_hz = TIMER_RESOLUTION_HZ;
  ESP_ERROR_CHECK(gptimer_new_timer(&config, &eu->timer));
  locate_in_timer_group(eu);

  // fire every period and start over from zero
  gptimer_alarm_config_t alarm = {};
  alarm.alarm_count = us;
  alarm.reload_count = 0;
  alarm.flags.auto_reload_on_alarm = true;
  ESP_ERROR_CHECK(gptimer_set_alarm_action(eu->timer, &alarm));

  gptimer_event_callbacks_t callbacks = {};
  callbacks.on_alarm = timer_isr;
  ESP_ERROR_CHECK(gptimer_register_event_callbacks(eu->timer, &callbacks, eu));

  ESP_ERROR_CHECK(gptimer_enable(eu->timer));
  ESP_ERROR_CHECK(gptimer_start(eu->timer));

  LOG("starting timer %" PRIu8 " with a period of %" PRIu32 " us", timer_id, us);

  // The period is an integer number of microsecond counts, so no rounding
  // happened.
  return eu->us_per_period;
}

uint32_t hal_elapsed_us_in_tick() {
  esp32_timer_t *const eu = get_timer(0);
  uint64_t count;
  ESP_ERROR_CHECK(gptimer_get_raw_count(eu->timer, &count));
  return (uint32_t)count;
}

bool hal_clock_interrupt_is_pending() {
  esp32_timer_t *const eu = get_timer(0);
  return timer_ll_get_intr_status(eu->hw) & TIMER_LL_EVENT_ALARM(eu->index);
}
