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
#include "periph/bss_canary/bss_canary.h"

#define LIGHT_EN GPIO_B7

bool lit = false;

void test_func(void *data) {
  lit = !lit;
  gpio_set_or_clr(LIGHT_EN, lit);
  schedule_us(1000000, (ActivationFuncPtr)test_func, NULL);
}

int main() {
  hal_init();

  init_clock(10000, TIMER1);
  gpio_make_output(LIGHT_EN);

  bss_canary_init();

  schedule_now((ActivationFuncPtr)test_func, NULL);

  cpumon_main_loop();
}
