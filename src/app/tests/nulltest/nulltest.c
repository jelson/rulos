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

#include "core/bss_canary.h"
#include "core/hardware.h"
#include "core/rulos.h"

#define FREQ_USEC 50000

#if defined(RULOS_ARM_LPC)
#define TEST_PIN GPIO0_08
#elif defined(RULOS_ARM_STM32)
#define TEST_PIN GPIO_A6
#elif defined(RULOS_AVR)
#define TEST_PIN GPIO_B3
#else
#error "No test pin defined"
#endif

void test_func(void *data) {
  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);
  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);
  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);
  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);
  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);
  gpio_set(TEST_PIN);
  gpio_clr(TEST_PIN);

  schedule_us(FREQ_USEC, (ActivationFuncPtr)test_func, NULL);
}

int main() {
  rulos_hal_init();

  init_clock(10000, TIMER1);
  gpio_make_output(TEST_PIN);
  bss_canary_init();
  schedule_now((ActivationFuncPtr)test_func, NULL);
  cpumon_main_loop();
}
