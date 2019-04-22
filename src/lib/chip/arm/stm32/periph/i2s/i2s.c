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
 * STM32 I2S support
 */

#include "core/hardware.h"
#include "core/hardware_types.h"
#include "core/rulos.h"
#include "periph/i2s/hal_i2s.h"

#if defined(RULOS_ARM_stm32f0)
#include "stm32f0xx_hal_i2s.h"
#include "stm32f0xx_ll_dma.h"
#elif defined(RULOS_ARM_stm32f1)
#include "stm32f1xx_hal_i2s.h"
#include "stm32f1xx_ll_dma.h"
#elif defined(RULOS_ARM_stm32f3)
#include "stm32f3xx_hal_i2s.h"
#include "stm32f3xx_ll_dma.h"
#else
#error "Your chip's definitions for I2C registers could use some help."
#include <stophere>
#endif

typedef struct {
  // init parameters from caller
  hal_i2s_play_done_cb_t play_done_cb;
  void* user_data;

  // internal state
  uint16_t* curr_buf;
  I2S_HandleTypeDef i2s_handle;
} stm32_i2s_t;

stm32_i2s_t stm32_i2s;

void hal_i2s_init(uint16_t sample_rate, hal_i2s_play_done_cb_t play_done_cb,
                  void* user_data) {
  memset(&stm32_i2s, 0, sizeof(stm32_i2s));
  stm32_i2s.play_done_cb = play_done_cb;
  stm32_i2s.user_data = user_data;

  // Initialize MCK
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF5_I2S;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // Initialize CK
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  // Initialize SD
  GPIO_InitStruct.Pin = GPIO_PIN_15;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  // Initialize WS
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  // Enable I2S Clock
  __HAL_RCC_SPI2_CLK_ENABLE();

  // Reset I2S Peripheral
  __HAL_RCC_SPI2_FORCE_RESET();
  __HAL_RCC_SPI2_RELEASE_RESET();

  /* I2S peripheral configuration */
  I2S_HandleTypeDef* const h = &stm32_i2s.i2s_handle;
  h->Instance = SPI2;
  h->Init.Mode = I2S_MODE_MASTER_TX;
  h->Init.Standard = I2S_STANDARD_PHILIPS;
  h->Init.DataFormat = I2S_DATAFORMAT_16B;
  h->Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
  h->Init.AudioFreq = sample_rate;
  h->Init.CPOL = I2S_CPOL_LOW;
  h->Init.ClockSource = I2S_CLOCK_SYSCLK;

  /* Disable and re-enable I2S block */
  __HAL_I2S_DISABLE(h);

  if (HAL_I2S_Init(h) != HAL_OK) {
    __builtin_trap();
  }
}

// Converts a 24-bit sample value to a 32-bit value in the proper
// format for the STM32 I2S peripheral to talk to the AK4430, which
// expects a 24-bit value padded out to 32.
uint32_t htoi2s_24(uint32_t v) {
  // uint32_t b3 = (v>>24) & 0xff;
  uint32_t b2 = (v >> 16) & 0xff;
  uint32_t b1 = (v >> 8) & 0xff;
  uint32_t b0 = (v)&0xff;
  return (b0 << 24) | (b2 << 8) | b1;
}

static void play_done_cb_trampoline(void* user_data) {
  stm32_i2s_t* stm32_i2s = (stm32_i2s_t*)user_data;
  stm32_i2s->play_done_cb(stm32_i2s->user_data, stm32_i2s->curr_buf);
}

void hal_i2s_play(uint16_t* samples, uint16_t num_samples) {
  stm32_i2s.curr_buf = samples;

  if (num_samples > 0) {
    if (HAL_I2S_Transmit(&stm32_i2s.i2s_handle, samples, num_samples,
                         HAL_MAX_DELAY) != HAL_OK) {
      __builtin_trap();
    }
  }

  schedule_now(play_done_cb_trampoline, &stm32_i2s);
}
