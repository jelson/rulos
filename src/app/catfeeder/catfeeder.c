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

/*
 * Basic test program that frobs a GPIO; should compile for all platforms. No
 * peripherals. Meant as a basic test of the build system for new platforms and
 * chips.
 */

#include "core/hardware.h"
#include "core/rulos.h"
#include "soc/timer_group_struct.h"

#define DIRECTION_PIN GPIO_2
#define MOVE_PIN      GPIO_3

#define MOVE_STEPS 100

static void tick(void *arg) {
  schedule_us(5000000, tick, arg);

  for (int i = 0; i < MOVE_STEPS; i++) {
    gpio_set(MOVE_PIN);
    hal_delay_ms(1);
    gpio_clr(MOVE_PIN);
    hal_delay_ms(1);
  }
}

int main() {
  rulos_hal_init();
  init_clock(10000, TIMER0);

  UartState_t uart;
  uart_init(&uart, /* uart_id= */ 0, 38400);
  log_bind_uart(&uart);

  LOG("Log output running!");

  schedule_now(tick, NULL);

  gpio_make_output(DIRECTION_PIN);
  gpio_clr(DIRECTION_PIN);
  gpio_make_output(MOVE_PIN);
  scheduler_run();
}
