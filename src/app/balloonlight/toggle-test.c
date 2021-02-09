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
 * This test program just turns the LED on and off periodically, making it easy
 * to measure light-on power vs light-off power
 */

#include "core/rulos.h"

#include <stdbool.h>
#include <stdint.h>

#include "stm32g0xx_ll_pwr.h"

#include "common.h"

static int led_counter = 0;

void toggle(void *data) {
  // This goes here instead of in main() because we want to give the
  // accelerometer sufficient turn-on time.
  if (data != NULL) {
    start_accel();
  }

  schedule_us(100000, (ActivationFuncPtr)toggle, NULL);

  led_counter++;
  if (led_counter == 5) {
    led_counter = 0;
  }

  gpio_set_or_clr(LIGHT_EN, led_counter == 0);
}


int main() {
  hal_init();

  init_clock(100000, TIMER1);
  gpio_make_output(LIGHT_EN);
  gpio_clr(LIGHT_EN);
  gpio_make_input_disable_pullup(ACCEL_INT);

  __HAL_RCC_PWR_CLK_ENABLE();

  // note: must be under 2mhz for this mode to work. See makefile for defines
  // that work in conjunction with stm32-hardware.c to set clock options to
  // 2mhz using HSI directly, disabling PLL.
  LL_PWR_EnableLowPowerRunMode();

  // The datasheet of the QMA7981 says the power-on time is 50 msec. We'll give
  // it 75 just to be safe.
  schedule_us(75000, (ActivationFuncPtr)toggle, (void *)1);

  cpumon_main_loop();
}
