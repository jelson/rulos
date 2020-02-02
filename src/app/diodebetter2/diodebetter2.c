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
#include "stm32f3xx_ll_rcc.h"

static void configure_pin_and_channel(TIM_HandleTypeDef *timerHandle,
				      uint32_t pin, uint32_t channel) {
  GPIO_InitTypeDef gpioStructure;
  gpioStructure.Mode = GPIO_MODE_AF_PP;
  gpioStructure.Alternate = GPIO_AF1_TIM2;
  gpioStructure.Pull = GPIO_NOPULL;
  gpioStructure.Speed = GPIO_SPEED_FREQ_HIGH;
  gpioStructure.Pin = pin;
  HAL_GPIO_Init(GPIOA, &gpioStructure);

  // Configure timer2 to generate a PWM output pulse. We set the pulse
  // width to 0 here because it's "manually" configured later by
  // poking a value directly into the CCR[1234] register of the timer.
  TIM_OC_InitTypeDef outputChannelInit = {
      0,
  };
  outputChannelInit.OCMode = TIM_OCMODE_PWM1;
  outputChannelInit.Pulse = 0;
  outputChannelInit.OCPolarity = TIM_OCPOLARITY_HIGH;
  outputChannelInit.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  outputChannelInit.OCFastMode = TIM_OCFAST_DISABLE;
  outputChannelInit.OCIdleState = TIM_OCIDLESTATE_RESET;
  outputChannelInit.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(timerHandle, &outputChannelInit,
				channel) != HAL_OK) {
    __builtin_trap();
  }
  if (HAL_TIM_PWM_Start(timerHandle, channel) != HAL_OK) {
    __builtin_trap();
  }
}

void init_pwm() {
  // Turn on the clock that drives timer2.
  __HAL_RCC_TIM2_CLK_ENABLE();

  // Set up timer2 with no prescaler. We expected this to run at 64mhz
  // (the chip's system clock frequency) but somewhere there's a
  // divide-by-2 prescaler we don't understand so it's running at
  // 32mhz. We set the period to be 16000 ticks, meaning a pulse will
  // be generated at 2khz (every 500usec).
  TIM_HandleTypeDef timerHandle;
  timerHandle.Instance = TIM2;
  timerHandle.Init.Prescaler = 1;
  timerHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
  timerHandle.Init.Period = 16000;
  timerHandle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  timerHandle.Init.RepetitionCounter = 0;
  timerHandle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&timerHandle) != HAL_OK) {
    __builtin_trap();
  }

  // Configure each of the four PWM channels and their corresponding
  // pins.
  configure_pin_and_channel(&timerHandle, GPIO_PIN_0, TIM_CHANNEL_1);
  configure_pin_and_channel(&timerHandle, GPIO_PIN_1, TIM_CHANNEL_2);
  configure_pin_and_channel(&timerHandle, GPIO_PIN_2, TIM_CHANNEL_3);
  configure_pin_and_channel(&timerHandle, GPIO_PIN_3, TIM_CHANNEL_4);
}

// This was a test to see if we could get even better resolution by
// using a faster-clocked timer. The stm32f303 reference manual says
// that TIM1 can be clocked directly from the PLL, supposedly x2,
// which in theory can give you a faster timer clock than the system
// clock. We're running the system clock at 64mhz so were hoping to
// get a 128mhz timer, but by selecting PLL clocking for tim1 only got
// a 64 mhz timer.
#if 1
void init_pwm_tim1test() {
  // First set up pin to use "alternate function" of timer2.
  GPIO_InitTypeDef gpioStructure;
  gpioStructure.Pin = GPIO_PIN_8;
  gpioStructure.Mode = GPIO_MODE_AF_PP;
  gpioStructure.Alternate = GPIO_AF6_TIM1;
  gpioStructure.Pull = GPIO_NOPULL;
  gpioStructure.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &gpioStructure);

  // Turn on the clock that drives timer2.
  __HAL_RCC_TIM1_CLK_ENABLE();

  // Set up timer2 with no prescaler (so, it runs at the chip's native
  // frequency of 64Mhz) and a period of 32000, meaning a pulse will
  // be generated at 2khz (every 500usec).
  TIM_HandleTypeDef timerHandle;
  timerHandle.Instance = TIM1;
  timerHandle.Init.Prescaler = 1;
  timerHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
  timerHandle.Init.Period = 16000;
  timerHandle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  timerHandle.Init.RepetitionCounter = 0;
  timerHandle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&timerHandle) != HAL_OK) {
    __builtin_trap();
  }
  // this is the magical line!
  LL_RCC_SetTIMClockSource(LL_RCC_TIM1_CLKSOURCE_PLL);

  // Configure timer2 to generate a PWM output pulse. We set the pulse
  // width to 0 here because it's "manually" configured later by
  // poking a value directly into the CCR[1234] register of the timer.
  TIM_OC_InitTypeDef outputChannelInit = {
      0,
  };
  outputChannelInit.OCMode = TIM_OCMODE_PWM1;
  outputChannelInit.Pulse = 50;
  outputChannelInit.OCPolarity = TIM_OCPOLARITY_HIGH;
  outputChannelInit.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  outputChannelInit.OCFastMode = TIM_OCFAST_DISABLE;
  outputChannelInit.OCIdleState = TIM_OCIDLESTATE_RESET;
  outputChannelInit.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&timerHandle, &outputChannelInit,
                                TIM_CHANNEL_1) != HAL_OK) {
    __builtin_trap();
  }

  if (HAL_TIM_PWM_Start(&timerHandle, TIM_CHANNEL_1) != HAL_OK) {
    __builtin_trap();
  }

  TIM1->CCR1 = 1;
}
#endif

// Configure the width of the PWM pulses, which are all in units of
// 1/64Mhz (about 16 nsec) clock ticks.
void pwm_adjust(uint32_t r, uint32_t g, uint32_t b, uint32_t w) {
  TIM2->CCR1 = w;
  TIM2->CCR2 = r;
  TIM2->CCR3 = g;
  TIM2->CCR4 = b;
}

#define num_brightnesses 42
static uint32_t bright_to_period[num_brightnesses];
void setup_table() {
    // 2^0, 2^1/3, 2^2/3 in 4-digit fixed-point
    uint32_t third_fix = 10000;
    uint32_t third_powers[] = {10000, 12599, 15874};
    for (uint32_t i=0; i<num_brightnesses; i++) {
        uint32_t full = i/3;
        uint32_t frac = i - full*3;
        if (i==0) {
            bright_to_period[i] = 0;
        } else {
            bright_to_period[i] = (1<<full) * third_powers[frac] / third_fix;
        }
    }
}

typedef struct {
  uint32_t phase;
} throb_state;

void set_period(uint32_t period) {
  pwm_adjust(period, period, period, period); 
  LOG("period now %d", period);
}

// Ramp brightness up over 5s, then back down.
void throb_act(void *arg) {
  throb_state* throb = (throb_state*) arg;
  throb->phase = (throb->phase + 1) % (num_brightnesses * 2);
  if (throb->phase < num_brightnesses) {
    set_period(bright_to_period[throb->phase]);
  } else {
    set_period(bright_to_period[2*num_brightnesses - throb->phase]);
  }
  schedule_us(116000, throb_act, throb);
}

int main() {
  hal_init();

  init_clock(10000, TIMER1);

  bss_canary_init();

  init_pwm();
  setup_table();
  pwm_adjust(1, 10, 100, 1000); 

  throb_state throb;
  schedule_now(throb_act, &throb);
  cpumon_main_loop();
}
