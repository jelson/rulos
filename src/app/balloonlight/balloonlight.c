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
 * This is a firmware for a simple motion-activated coin-cell light based on the
 * STM32G031 CPU and the QMA7981 accelerometer. I picked the STM32G031 because I
 * like the STM32 line, the G0 is a fantastic new series that's incredibly
 * capable but cheap, and ST recently introced a tiny and incredibly low-cost
 * 8-pin version (the STM32G031J6M6). LCSC sells it for just 55 cents.
 *
 * I picked the QMA7981 accelerometer for no particular reason other than that
 * it's the cheapest acclerometer on LCSC. It has low quiescent power when
 * sampling at low rate and can generate interrupts on motion.
 *
 * The program doesn't do much: it configures the accelerometer to generate
 * interrupts when motion is detected on any of the three axes, then turns on
 * the light. The light is controlled by a combination boost-converter LED
 * driver, the MT9284, which can boost the voltage of the coin cell enough to
 * light up to 6 white LEDs in series. It has an enable pin.
 *
 * The goal is to keep idle power to just a few microamps so that the device can
 * last for a long time (years) on a single coin cell. The STM32 has low-power
 * modes, e.g. STANDBY mode in which the device can be waken by an external
 * interrupt on a special pin (WKUP) and only uses 300nA if ULPEN=1.
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

// Initialize with the timeout already set in the future. The program starts
// running in response to an accelerometer interrupt, so the light should be
// turned on immediately.
uint32_t led_on_time_ms = LIGHT_TIMEOUT_SEC;

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

  // if motion has stopped, turn off the LED and go to sleep
  if (led_on_time_ms == 0) {
    // Turn off the LED
    gpio_clr(LIGHT_EN);

    // Set the sleep-mode pulldown on the LED driver control line
    HAL_PWREx_EnablePullUpPullDownConfig();
    HAL_PWREx_EnableGPIOPullDown(PWR_GPIO_B, PWR_GPIO_BIT_7);

    // Configure shutdown to wake up if the accel int line goes high
    HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN6_HIGH);
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WUF6);

    // Go into super-low-power shutdown mode
    HAL_PWREx_EnterSHUTDOWNMode();
    // HAL_PWR_EnterSTANDBYMode();
  }
}

int main() {
  hal_init();

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
