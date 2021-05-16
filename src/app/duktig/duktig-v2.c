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
#include "stm32g0xx_hal_rcc.h"
#include "stm32g0xx_ll_exti.h"
#include "stm32g0xx_ll_pwr.h"
#include "stm32g0xx_ll_rcc.h"
#include "stm32g0xx_ll_tim.h"

#define BUT1   GPIO_A2
#define BUT2   GPIO_A4
#define LED1   GPIO_A0
#define LED2   GPIO_A6
#define PWRLED GPIO_B3

#define JIFFY_TIME_US          10000
#define KEY_REFRACTORY_TIME_US 10000
#define TIMEOUT_WHILE_ON_US    (1000000 * 120)  // 120 sec
#define PWRLED_BLINK_TIME_US   250000

// Low-power mode fun of the stm32g0!
//
// The non-microcontroller ICs on the board account for roughly 1.5uA or so.
//
// In STOP1 mode the datasheet claims ~3.5uA. In this mode the chip responds to
// interrupts, so we set up the EXTI controller to generate interrupts on the
// falling edges of the button GPIOs and create interrupt handlers that don't do
// anything but clear the interrupt flags. Total power use: 4.7-4.9uA.
//
// In STANDBY mode the datasheet claims ~300nA for the STM32G031, about 1.5uA
// for the STM32G030. However, normal GPIO stops working. The chip can only be
// waken up (externally) by special Wakeup pins (WKUP) that can be configured to
// be sensitive to either being high or low. Normal GPIO pullup/pulldown
// configuration doesn't work any more either, but there's a special register
// that allows pullup and pulldowns to be configured during standby mode. Total
// power use: 2.9-3.0uA.
//
// In SHUTDOWN mode we have the datasheet claims, amazingly, ~30nA. There are
// some differences between what works in STANDBY vs SHUTDOWN but those don't
// seem to be relevant for this application. Total power use: 1.7-1.8uA.
// However, while the chip is booting from SHUTDOWN there's a transient where
// the GPIO pins are not driven and there are visible artifacts -- the LEDs
// momentarily light. To use this mode we'd need external pulldowns on the LED
// driver control pins.
//
// There's another dimension to consider, which is cost. If we use STOP1, the
// buttons can be attached to any pin and we can use the 8-pin STM32G030J6M6,
// which costs 71 cents in Qty 150 (Aug 2020). But the 8-pin version only
// exposes a single WKUP pin, so STANDBY/SHUTDOWN can not be used. We'd instead
// need to use the 20-pin STM32G030F6P6, which costs 82 cents in Qty 150. (Or,
// use some external diodes so pressing either button generates a pulse on the
// single WKUP pin.) Also, for a board this simple, it's less humiliating to use
// an 8-pin micro rather than a 20-pin.
//
// Cost matrix at Qty 100, Aug 2020:
//
//             8-pin       20-pin
// STM32G030   $0.71        $0.82
// STM32G031   $1.07        $1.36
//
// Power matrix, uA:
//
//              STOP1     STANDBY     SHUTDOWN
// STM32G030     4.9        3.0          n/a
// STM32G031     4.9        3.0          1.8
//
// For reference, the original board, which used a complex arrangement of
// discrete transistors and diodes to cut off power to the board completely
// until a button was pressed, idles at about 2.3uA. Nearly all of that is
// coming from the 5v voltage regulator, an MCP1702. Interesting question: how
// much lower could I get this new board if I only power the CPU continuously,
// and use a power transistor to cut off power to everything else, such as the
// LED drivers?
//

#define USE_SHUTDOWN 1

#define LIGHT_OFF  0
#define LIGHT_LOW  1
#define LIGHT_HIGH 2

typedef struct {
  uint32_t light1_state;
  uint32_t light2_state;
  DebouncedButton_t but1;
  DebouncedButton_t but2;
  Time shutdown_time;

  bool pwrled_on;
  int pwrled_count;
  Time pwrled_next_transition;
} DuktigState_t;

DuktigState_t duktig;

#if USE_SHUTDOWN == 0

void EXTI2_3_IRQHandler(void) {
  if (LL_EXTI_IsActiveFallingFlag_0_31(LL_EXTI_LINE_2)) {
    LL_EXTI_ClearFallingFlag_0_31(LL_EXTI_LINE_2);
  }
}

void EXTI4_15_IRQHandler(void) {
  if (LL_EXTI_IsActiveFallingFlag_0_31(LL_EXTI_LINE_4)) {
    LL_EXTI_ClearFallingFlag_0_31(LL_EXTI_LINE_4);
  }
}

#endif  // SHUTDOWN

void power_down() {
#if USE_SHUTDOWN
  // needed for standby mode, which is lower power than STOP mode
  HAL_PWREx_EnablePullUpPullDownConfig();

  // Set the pullups on the buttons
  HAL_PWREx_EnableGPIOPullUp(PWR_GPIO_A, PWR_GPIO_BIT_2 | PWR_GPIO_BIT_4);

  // Set the pulldowns on the LED driver control lines
  HAL_PWREx_EnableGPIOPullDown(PWR_GPIO_A, PWR_GPIO_BIT_0 | PWR_GPIO_BIT_6);

  // Configure standby to wake up if a button goes low
  HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN2_LOW);
  HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN4_LOW);
  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WUF2);
  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WUF4);

  HAL_PWR_EnterSTANDBYMode();
  // SHUTDOWN: not available on the stm32g0x0
  // HAL_PWREx_EnterSHUTDOWNMode();
#else   // USE_SHUTDOWN
  HAL_SuspendTick();
  HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
  HAL_ResumeTick();
#endif  // USE_SHUTDOWN
}

static uint32_t next_light_state(uint32_t curr_light_state) {
  if (curr_light_state == LIGHT_HIGH) {
    return LIGHT_OFF;
  } else {
    return curr_light_state + 1;
  }
}

static void duktig_update(DuktigState_t *duktig) {
  schedule_us(JIFFY_TIME_US, (ActivationFuncPtr)duktig_update, duktig);

  // sample the buttons
  bool but1_pushed = debounce_button(&duktig->but1, gpio_is_clr(BUT1));
  bool but2_pushed = debounce_button(&duktig->but2, gpio_is_clr(BUT2));

  // If we've reached the light-on timeout, turn off the lights
  if (later_than(clock_time_us(), duktig->shutdown_time)) {
    duktig->light1_state = LIGHT_OFF;
    duktig->light2_state = LIGHT_OFF;
  }

  if (but1_pushed) {
    duktig->light1_state = next_light_state(duktig->light1_state);
  }

  if (but2_pushed) {
    duktig->light2_state = next_light_state(duktig->light2_state);
  }

  const bool but_pushed = but1_pushed || but2_pushed;
  const bool light_on =
      duktig->light1_state != LIGHT_OFF || duktig->light2_state != LIGHT_OFF;

  // If a light was just turned on, set the timeout to turn it back off
  if (but_pushed && light_on) {
    duktig->shutdown_time = clock_time_us() + TIMEOUT_WHILE_ON_US;
  }

  // If a button was pushed, blink the power indicator 3 times
  if (but_pushed) {
    duktig->pwrled_count = 2;
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
  if (!light_on && duktig->pwrled_count == 0) {
    power_down();
  }
}

int refresh_count = 0;

void TIM3_IRQHandler() {
  if (LL_TIM_IsActiveFlag_UPDATE(TIM3)) {
    LL_TIM_ClearFlag_UPDATE(TIM3);
  }

  refresh_count++;
  if (refresh_count == 10) {
    refresh_count = 0;
  }

  switch (duktig.light1_state) {
    case LIGHT_OFF:
      gpio_clr(LED1);
      break;
    case LIGHT_HIGH:
      gpio_set(LED1);
      break;
    case LIGHT_LOW:
      if (refresh_count == 0) {
        gpio_set(LED1);
      } else {
        gpio_clr(LED1);
      }
      break;
  }

  switch (duktig.light2_state) {
    case LIGHT_OFF:
      gpio_clr(LED2);
      break;
    case LIGHT_HIGH:
      gpio_set(LED2);
      break;
    case LIGHT_LOW:
      if (refresh_count == 0) {
        gpio_set(LED2);
      } else {
        gpio_clr(LED2);
      }
      break;
  }
}

int main() {
  hal_init();

  // set up output pins as drivers
  gpio_clr(LED1);
  gpio_clr(LED2);
  gpio_clr(PWRLED);
  gpio_make_output(LED1);
  gpio_make_output(LED2);
  gpio_make_output(PWRLED);

  __HAL_RCC_PWR_CLK_ENABLE();

  // note: must be under 2mhz for this mode to work. See makefile for defines
  // that work in conjunction with stm32-hardware.c to set clock options to
  // 2mhz using HSI directly, disabling PLL.
  LL_PWR_EnableLowPowerRunMode();

  // init state
  memset(&duktig, 0, sizeof(duktig));

  // set up buttons
  gpio_make_input_enable_pullup(BUT1);
  debounce_button_init(&duktig.but1, KEY_REFRACTORY_TIME_US);
  gpio_make_input_enable_pullup(BUT2);
  debounce_button_init(&duktig.but2, KEY_REFRACTORY_TIME_US);

#if USE_SHUTDOWN == 0
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
#endif

  // Initialize the refresh timer
  __HAL_RCC_TIM3_CLK_ENABLE();
  HAL_NVIC_SetPriority(TIM3_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(TIM3_IRQn);
  LL_TIM_InitTypeDef timer_init;
  timer_init.Prescaler = 0;
  timer_init.CounterMode = LL_TIM_COUNTERMODE_UP;
  // 500 counts = 4khz. Chip configured to run at 2mhz, but clocks
  // run at 2x core speed (I think)
  timer_init.Autoreload = 500;
  timer_init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  timer_init.RepetitionCounter = 0;
  LL_TIM_Init(TIM3, &timer_init);
  LL_TIM_SetTriggerOutput(TIM3, LL_TIM_TRGO_UPDATE);
  LL_TIM_DisableMasterSlaveMode(TIM3);
  LL_TIM_EnableIT_UPDATE(TIM3);
  LL_TIM_EnableCounter(TIM3);
  LL_TIM_GenerateEvent_UPDATE(TIM3);

  // set up periodic sampling task
  init_clock(JIFFY_TIME_US, TIMER1);
  schedule_us(1, (ActivationFuncPtr)duktig_update, &duktig);

  cpumon_main_loop();
  assert(FALSE);
}
