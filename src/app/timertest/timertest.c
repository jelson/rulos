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

#define FREQ_USEC 50000
#define TOTAL 20
#define TEST_HAL

#ifdef RULOS_ARM
#define TEST_PIN GPIO0_00
#else
#define TEST_PIN GPIO_B5
#endif

#if TEST_SCHEDULER
void test_func(void *data) {
  gpio_set(TEST_PIN);
  schedule_us(FREQ_USEC, a);
  gpio_clr(TEST_PIN);
}
#endif

void hal_test_func(void *data) {
  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);
}

int main() {
  hal_init();
  gpio_make_output(TEST_PIN);
  gpio_clr(TEST_PIN);

#ifdef TEST_HAL
  hal_start_clock_us(FREQ_USEC, hal_test_func, NULL, TIMER1);
  while (1) {
  };

#endif

#ifdef TEST_SCHEDULER
  init_clock(FREQ_USEC, TIMER1);

  a.func = test_func;

  int i;
  for (i = 0; i < TOTAL - 1; i++) schedule_us(1000000 + i, &a);

  schedule_now(&a);

  CpumonAct cpumon;
  cpumon_init(&cpumon);
  cpumon_main_loop();
#endif
}
