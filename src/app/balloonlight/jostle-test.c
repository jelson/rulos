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
 * A preliminary test of the accelerometer interrupt generation and interrupt
 * input pin - but simply sampling the pin rather than having it generate an
 * stm32 interrupt
 */

#include "common.h"
#include "core/hardware.h"
#include "core/rulos.h"
#include "stm32g0xx_hal_rcc.h"
#include "stm32g0xx_ll_exti.h"
#include "stm32g0xx_ll_i2c.h"
#include "stm32g0xx_ll_pwr.h"

////// configuration //////

#define LIGHT_TIMEOUT_SEC  5
#define SAMPLING_TIME_MSEC 10

uint32_t led_on_time_ms = 0;

void sample(void *data) {
  // This goes here instead of in main() because we want to give the
  // accelerometer sufficient turn-on time.
  if (data != NULL) {
    start_accel();
  }

  schedule_us(SAMPLING_TIME_MSEC * 1000, (ActivationFuncPtr)sample, NULL);

  if (gpio_is_set(ACCEL_INT)) {
    // motion
    led_on_time_ms = LIGHT_TIMEOUT_SEC * 1000;
  } else {
    // no motion
    if (led_on_time_ms > 0) {
      led_on_time_ms -= SAMPLING_TIME_MSEC;
    }
  }

  gpio_set_or_clr(LIGHT_EN, led_on_time_ms > 0);
}

int main() {
  rulos_hal_init();

  init_clock(SAMPLING_TIME_MSEC * 1000, TIMER1);

  __HAL_RCC_PWR_CLK_ENABLE();

  // note: must be under 2mhz for this mode to work. See makefile for defines
  // that work in conjunction with stm32-hardware.c to set clock options to
  // 2mhz using HSI directly, disabling PLL.
  LL_PWR_EnableLowPowerRunMode();

  // The datasheet of the QMA7981 says the power-on time is 50 msec. We'll give
  // it 75 just to be safe.
  schedule_us(75000, (ActivationFuncPtr)sample, (void *)1);

  cpumon_main_loop();
}
