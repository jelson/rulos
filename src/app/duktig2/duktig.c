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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "core/clock.h"
#include "core/hardware.h"
#include "core/rulos.h"
#include "core/util.h"

#include "stm32g0xx_ll_rcc.h"
#include "stm32g0xx_ll_pwr.h"
#include "stm32g0xx_ll_exti.h"

#define BUT1 GPIO_A2
#define BUT2 GPIO_A4
#define LED1 GPIO_A0
#define LED2 GPIO_A6
#define PWRLED GPIO_B3

#define JIFFY_TIME_US 10000
#define KEY_REFRACTORY_TIME_US 10000
#define TIMEOUT_WHILE_ON_US (1000000 * 120) // 120 sec
#define PWRLED_BLINK_TIME_US 250000

typedef struct {
  r_bool light1_on;
  r_bool light2_on;
  DebouncedButton_t but1;
  DebouncedButton_t but2;
  Time shutdown_time;

  r_bool pwrled_on;
  int pwrled_count;
  Time pwrled_next_transition;
} DuktigState_t;

volatile int num_int = 0;

void EXTI2_3_IRQHandler(void) {
  if (LL_EXTI_IsActiveFallingFlag_0_31(LL_EXTI_LINE_2)) {
    LL_EXTI_ClearFallingFlag_0_31(LL_EXTI_LINE_2);
    num_int++;
  }
}

void EXTI4_15_IRQHandler(void) {
  if (LL_EXTI_IsActiveFallingFlag_0_31(LL_EXTI_LINE_4)) {
    LL_EXTI_ClearFallingFlag_0_31(LL_EXTI_LINE_4);
    num_int++;
  }
}

void power_down() {
  HAL_SuspendTick();
  HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
  //HAL_PWR_EnterSTANDBYMode();
  HAL_ResumeTick();
}


static void duktig_update(DuktigState_t *duktig) {
  schedule_us(JIFFY_TIME_US, (ActivationFuncPtr)duktig_update, duktig);

  // sample the buttons
  r_bool but1_pushed = debounce_button(&duktig->but1, gpio_is_clr(BUT1));
  r_bool but2_pushed = debounce_button(&duktig->but2, gpio_is_clr(BUT2));

  // If we've reached the light-on timeout, turn off the lights
  if (later_than(clock_time_us(), duktig->shutdown_time)) {
    duktig->light1_on = false;
    duktig->light2_on = false;
  }

  if (but1_pushed) {
    duktig->light1_on = !duktig->light1_on;
  }

  if (but2_pushed) {
    duktig->light2_on = !duktig->light2_on;
  }

  gpio_set_or_clr(LED2, duktig->light2_on);
  gpio_set_or_clr(LED1, duktig->light1_on);

  // If a light was just turned on, set the timeout to turn it back off
  if ((but1_pushed || but2_pushed) &&
      (duktig->light1_on || duktig->light2_on)) {
    duktig->shutdown_time = clock_time_us() + TIMEOUT_WHILE_ON_US;
  }

  // if a button was pushed, blink the power indicator 3 times
  if (but1_pushed || but2_pushed) {
    duktig->pwrled_count = 3;
    duktig->pwrled_next_transition = clock_time_us();
  }

  // advance the pwrled flasher, if there's been a recent flash
  if (duktig->pwrled_count > 0 &&
      later_than(clock_time_us(), duktig->pwrled_next_transition)) {
    if (duktig->pwrled_on) {
      gpio_clr(PWRLED);
      duktig->pwrled_on = false;
      duktig->pwrled_count--;
    } else {
      gpio_set(PWRLED);
      duktig->pwrled_on = true;
    }

    if (duktig->pwrled_count > 0) {
      duktig->pwrled_next_transition = clock_time_us() + PWRLED_BLINK_TIME_US;
    }
  }

  // if we're off, go to sleep
  if (!duktig->light1_on && !duktig->light2_on && duktig->pwrled_count == 0) {
    power_down();
  }
}

int main() {
  hal_init();
  __HAL_RCC_PWR_CLK_ENABLE();

  // note: must be under 2mhz for this mode to work. See makefile for defines
  // that work in conjunction with stm32-hardware.c to set clock options to
  // 2mhz using HSI directly, disabling PLL.
  LL_PWR_EnableLowPowerRunMode();

  // init state
  DuktigState_t duktig;
  memset(&duktig, 0, sizeof(duktig));

  // set up output pins as drivers
  gpio_make_output(LED1);
  gpio_make_output(LED2);
  gpio_make_output(PWRLED);

  // set up buttons
  gpio_make_input_enable_pullup(BUT1);
  debounce_button_init(&duktig.but1, KEY_REFRACTORY_TIME_US);
  gpio_make_input_enable_pullup(BUT2);
  debounce_button_init(&duktig.but2, KEY_REFRACTORY_TIME_US);

#if 0
  // needed for standby mode (which we're not using at the moment), but seems to
  // break stop mode
  HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1_LOW);
  HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN4_LOW);
#endif

  // Set up interrupts to fire when buttons are pushed. These will wake us up
  // from STOP mode.
  LL_EXTI_SetEXTISource(LL_EXTI_CONFIG_PORTA, LL_EXTI_CONFIG_LINE2);
  LL_EXTI_InitTypeDef EXTI_InitStruct = {0};
  EXTI_InitStruct.Line_0_31 = LL_EXTI_LINE_2;
  EXTI_InitStruct.LineCommand = ENABLE;
  EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;
  EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_FALLING;
  LL_EXTI_Init(&EXTI_InitStruct);

  LL_EXTI_SetEXTISource(LL_EXTI_CONFIG_PORTA, LL_EXTI_CONFIG_LINE4);
  EXTI_InitStruct.Line_0_31 = LL_EXTI_LINE_4;
  EXTI_InitStruct.LineCommand = ENABLE;
  EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;
  EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_FALLING;
  LL_EXTI_Init(&EXTI_InitStruct);

  NVIC_SetPriority(EXTI2_3_IRQn, 3);
  NVIC_EnableIRQ(EXTI2_3_IRQn);
  NVIC_SetPriority(EXTI4_15_IRQn, 3);
  NVIC_EnableIRQ(EXTI4_15_IRQn);

  // set up periodic sampling task
  init_clock(JIFFY_TIME_US, TIMER1);
  schedule_us(1, (ActivationFuncPtr)duktig_update, &duktig);

  cpumon_main_loop();
  assert(FALSE);
}
