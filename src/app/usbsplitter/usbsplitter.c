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

#define OUTPUT1 GPIO_A6
#define OUTPUT2 GPIO_A4

#define TICK_LENGTH_US      10
#define TICKS_ON_PER_OUTPUT 3
#define TICKS_ALL_OFF       1

#define TICKS_PER_CYCLE (2 * TICKS_ON_PER_OUTPUT + TICKS_ALL_OFF)

static uint32_t curr_tick = 0;

void switch_outputs(void *data) {
  curr_tick++;
  if (curr_tick == TICKS_PER_CYCLE) {
    curr_tick = 0;
  }

  if (curr_tick == 0) {
    gpio_clr(OUTPUT2);
    gpio_set(OUTPUT1);
  } else if (curr_tick == TICKS_ON_PER_OUTPUT) {
    gpio_clr(OUTPUT1);
    gpio_set(OUTPUT2);
  } else if (curr_tick == 2 * TICKS_ON_PER_OUTPUT) {
    gpio_clr(OUTPUT1);
    gpio_clr(OUTPUT2);
  }
}

int main() {
  rulos_hal_init();

  gpio_make_output(OUTPUT1);
  gpio_make_output(OUTPUT2);

  // Since this is such a simple program we'll just use the HAL clock
  // directly and not bother with the scheduler
  hal_start_clock_us(TICK_LENGTH_US, switch_outputs, NULL, 0);

  while (true) {
    __WFI();
  }
}
