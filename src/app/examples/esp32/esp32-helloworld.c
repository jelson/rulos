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
#include "periph/uart/uart.h"

#define JIFFY_CLOCK_US  10000   // 10 ms jiffy clock
#define MESSAGE_FREQ_US 500000  // how often to print a message

#define TEST_PIN GPIO_2

static int line = 0;

static void test_func(void *data) {
  schedule_us(MESSAGE_FREQ_US, test_func, data);

  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);
  LOG("Hello world from esp32, line %d at time %d", line++, clock_time_us());
}

int main() {
  rulos_hal_init();
  init_clock(JIFFY_CLOCK_US, TIMER0);

  UartState_t u;
  uart_init(&u, /* uart_id= */ 0, 38400);
  log_bind_uart(&u);

  gpio_make_output(TEST_PIN);
  gpio_clr(TEST_PIN);
  schedule_now(test_func, NULL);
  cpumon_main_loop();
}
