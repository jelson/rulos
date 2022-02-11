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

#include "core/hardware.h"
#include "core/rulos.h"

#define FREQ_USEC 500000
#define NUM_TASKS 20

//#define TEST_HAL
#define TEST_SCHEDULER

#include "../test-pin.h"

#ifdef LOG_TO_SERIAL
#include "periph/uart/uart.h"
#endif

#ifdef TEST_SCHEDULER
static void test_func(void *data) {
  static int i = 0;

  gpio_set(TEST_PIN);
  schedule_us(FREQ_USEC * NUM_TASKS, test_func, data);
  gpio_clr(TEST_PIN);
  LOG("line %d printed by task %d at time %" PRId32, i++, (int)data,
      clock_time_us());
}
#endif

void hal_test_func(void *data) {
  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);
}

int main() {
#ifdef LOG_TO_SERIAL
  UartState_t u;
  uart_init(&u, /* uart_id= */ 0, 38400);
  log_bind_uart(&u);
#endif

  rulos_hal_init();
  gpio_make_output(TEST_PIN);
  gpio_clr(TEST_PIN);

#ifdef TEST_HAL
  hal_start_clock_us(FREQ_USEC, hal_test_func, NULL, TIMER1);
  while (1) {
  };

#endif

#ifdef TEST_SCHEDULER
  init_clock(FREQ_USEC, TIMER1);

  for (int i = 0; i < NUM_TASKS; i++) {
    schedule_us(FREQ_USEC * (i + 1), test_func, (void *)i);
  }

  schedule_now(test_func, (void *)-1);

  scheduler_run();
#endif
}
