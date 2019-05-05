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
#include "stm32f3xx_ll_bus.h"
#include "stm32f3xx_ll_dma.h"
#include "stm32f3xx_ll_rcc.h"
#include "stm32f3xx_ll_spi.h"
#else
#error "Your chip's definitions for I2C registers could use some help."
#include <stophere>
#endif

typedef struct {
  // init parameters from caller
  hal_i2s_play_done_cb_t play_done_cb;
  void* user_data;

  // internal state
  I2S_HandleTypeDef i2s_handle;
} stm32_i2s_t;

stm32_i2s_t stm32_i2s;

static void call_play_done(uint8_t just_played_idx) {
  stm32_i2s.play_done_cb(stm32_i2s.user_data, just_played_idx);
}

void DMA1_Channel5_IRQHandler() {
  if (LL_DMA_IsActiveFlag_HT5(DMA1)) {
    // "Half-transmit" interrupt: means we finished transmitting the
    // first half of the buffer.
    LL_DMA_ClearFlag_HT5(DMA1);
    call_play_done(0);
  }
  if (LL_DMA_IsActiveFlag_TC5(DMA1)) {
    // "Transmit-complete" interrupt: means we finished transmitting
    // the entire buffer, i.e. the second half has now been
    // transmitted.
    LL_DMA_ClearFlag_TC5(DMA1);
    call_play_done(1);
  }

  LL_DMA_ClearFlag_GI5(DMA1);
}

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

  /* Disable, configure and re-enable the I2S block */
  __HAL_I2S_DISABLE(h);
  if (HAL_I2S_Init(h) != HAL_OK) {
    __builtin_trap();
  }
  __HAL_I2S_ENABLE(h);

  // Set up DMA
  __HAL_RCC_DMA1_CLK_ENABLE();
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
  NVIC_SetPriority(DMA1_Channel5_IRQn, 0);
  NVIC_EnableIRQ(DMA1_Channel5_IRQn);
  LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_5);
  LL_SPI_DisableDMAReq_TX(SPI2);
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

void hal_i2s_start(uint16_t* samples, uint16_t num_samples_per_halfbuffer) {
  LL_DMA_ClearFlag_HT5(DMA1);
  LL_DMA_ClearFlag_TC5(DMA1);

  LL_DMA_ConfigTransfer(
      DMA1, LL_DMA_CHANNEL_5,
      LL_DMA_DIRECTION_MEMORY_TO_PERIPH | LL_DMA_PRIORITY_HIGH |
          LL_DMA_MODE_CIRCULAR | LL_DMA_PERIPH_NOINCREMENT |
          LL_DMA_MEMORY_INCREMENT | LL_DMA_PDATAALIGN_HALFWORD |
          LL_DMA_MDATAALIGN_HALFWORD);
  LL_DMA_ConfigAddresses(
      DMA1, LL_DMA_CHANNEL_5, (uint32_t)samples, LL_SPI_DMA_GetRegAddr(SPI2),
      LL_DMA_GetDataTransferDirection(DMA1, LL_DMA_CHANNEL_5));
  LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_5, 2 * num_samples_per_halfbuffer);

  LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_5);
  LL_DMA_EnableIT_HT(DMA1, LL_DMA_CHANNEL_5);
  LL_SPI_EnableDMAReq_TX(SPI2);
  LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_5);
}

void hal_i2s_stop() {
  LL_SPI_DisableDMAReq_TX(SPI2);
  LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_5);
  LL_DMA_DisableIT_TC(DMA1, LL_DMA_CHANNEL_5);
  LL_DMA_DisableIT_HT(DMA1, LL_DMA_CHANNEL_5);
  LL_DMA_ClearFlag_HT5(DMA1);
  LL_DMA_ClearFlag_TC5(DMA1);
}
