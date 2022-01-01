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

#define CONSOLE_UART_NUM 0

UartState_t console;

int main() {
  hal_init();
  init_clock(10000, TIMER1);

  // initialize console uart
  uart_init(&console, CONSOLE_UART_NUM, 1000000);
  log_bind_uart(&console);
  LOG("RTC test starting");

  Time last_time = precise_clock_time_us();
  for (int i = 0; i < 10000000; i++) {
    Time curr_time = precise_clock_time_us();
    if (curr_time < last_time) {
      LOG("test %d failed! time %ld came before time %ld", i, last_time,
          curr_time);
      goto done;
    }
    last_time = curr_time;
    if (i % 100000 == 0) {
      LOG("%d tests successful...", i);
    }
  }
  LOG("success!");

done:
  while (1) {
  }
}
