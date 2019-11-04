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

ADC_HandleTypeDef hadc1;

void init_pwm() {
  GPIO_InitTypeDef gpioStructure;
  gpioStructure.Pin = GPIO_PIN_6;
  gpioStructure.Mode = GPIO_MODE_AF_OD;
  gpioStructure.Pull = GPIO_NOPULL;
  gpioStructure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpioStructure.Alternate = GPIO_AF1_TIM3;
  HAL_GPIO_Init(GPIOA, &gpioStructure);  
}

void init_adc() {
  __HAL_RCC_ADC_CLK_ENABLE();

  RCC_PeriphCLKInitTypeDef PeriphClkInit;

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCCLKSOURCE_SYSCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
    __builtin_trap();
  }

  gpio_make_adc_input(GPIO_A5);
  
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.LowPowerAutoPowerOff = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc1.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_39CYCLES_5;
  hadc1.Init.SamplingTimeCommon2 = ADC_SAMPLETIME_160CYCLES_5;
  hadc1.Init.OversamplingMode = ENABLE;
  hadc1.Init.Oversampling.Ratio         = ADC_OVERSAMPLING_RATIO_256;
  hadc1.Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_8;
  hadc1.Init.Oversampling.TriggeredMode = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
   
  hadc1.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
  if (HAL_ADC_Init(&hadc1) != HAL_OK) {
    __builtin_trap();
  }

  ADC_ChannelConfTypeDef sConfig = {0};
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
    __builtin_trap();
  }

  /* Run the ADC calibration */
  if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK) {
    __builtin_trap();
  }
}

void pwm_adjust(uint32_t pulse_interval, uint32_t pulse_width) {
  __HAL_RCC_TIM3_CLK_ENABLE();
 
  TIM_HandleTypeDef timerHandle;
  timerHandle.Instance = TIM3;
  timerHandle.Init.Prescaler = 40;
  timerHandle.Init.CounterMode = TIM_COUNTERMODE_UP;
  timerHandle.Init.Period = pulse_interval;
  timerHandle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  timerHandle.Init.RepetitionCounter = 0;
  timerHandle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&timerHandle) != HAL_OK) {
    __builtin_trap();
  }

  TIM_OC_InitTypeDef outputChannelInit = {0,};
  outputChannelInit.OCMode = TIM_OCMODE_PWM1;
  outputChannelInit.Pulse = pulse_width;
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
}


void sample(void *arg) {
  schedule_us(50000, sample, NULL);

  HAL_ADC_Start(&hadc1);
  if (HAL_ADC_PollForConversion(&hadc1, 1000000) != HAL_OK) {
    __builtin_trap();
  }
  volatile uint32_t val = HAL_ADC_GetValue(&hadc1);
  HAL_ADC_Stop(&hadc1);

  TIM3->CCR1 = 1600 + (1600 * val / (1<<12));
}

int main() {
  hal_init();

  init_clock(10000, TIMER1);

  bss_canary_init();

  init_pwm();
  init_adc();
  pwm_adjust(32000, 0);

  schedule_now(sample, NULL);
  cpumon_main_loop();
}
