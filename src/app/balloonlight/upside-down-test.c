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
 * This test program turns on the LED when the device is upside down, and off
 * again when it's right-side up
 */

#include <stdbool.h>
#include <stdint.h>

#include "common.h"
#include "core/rulos.h"
#include "stm32g0xx_ll_pwr.h"

////// configuration //////

#define SAMPLING_TIME_MSEC 10

static int16_t last_accel[3] = {};
static int16_t ewma_accel[3] = {};
static bool led_is_on = false;

void sample(void *data) {
  // This goes here instead of in main() because we want to give the
  // accelerometer sufficient turn-on time.
  if (data != NULL) {
    start_accel();
  }

  schedule_us(SAMPLING_TIME_MSEC * 1000, (ActivationFuncPtr)sample, NULL);

  read_accel_values(last_accel);

  for (int i = 0; i < 3; i++) {
    ewma_accel[i] = (last_accel[i] / 4) + (3 * (ewma_accel[i] / 4));
  }

  if (led_is_on) {
    if (ewma_accel[2] > 100) {
      led_is_on = false;
    }
  } else {
    if (ewma_accel[2] < -100) {
      led_is_on = true;
    }
  }

  gpio_set_or_clr(LIGHT_EN, led_is_on);
}

int main() {
  rulos_hal_init();

  init_clock(100000, TIMER1);

  __HAL_RCC_PWR_CLK_ENABLE();

  // note: must be under 2mhz for this mode to work. See makefile for defines
  // that work in conjunction with stm32-hardware.c to set clock options to
  // 2mhz using HSI directly, disabling PLL.
  LL_PWR_EnableLowPowerRunMode();

  // The datasheet of the QMA7981 says the power-on time is 50 msec. We'll give
  // it 75 just to be safe.
  schedule_us(75000, (ActivationFuncPtr)sample, (void *)1);

  scheduler_run();
}
