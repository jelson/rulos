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

#include "core/clock.h"
#include "core/network.h"
#include "core/rulos.h"
#include "core/util.h"
#include "periph/audio/audio_driver.h"
#include "periph/audio/audio_server.h"
#include "periph/audio/audio_streamer.h"
#include "periph/audio/audioled.h"
#include "periph/sdcard/sdcard.h"
#include "periph/uart/serial_console.h"

/*
 * PIN MAPPINGS:
 *
 * STM32F303CB   STM32Func  AK4430    AudioFunction
 * -----------   ---------- -------   --------------
 * 29/PA8  AF5   I2S2_MCK      4           MCLK
 * 26/PB13 AF5   I2S2_CK       5           BICK
 * 28/PB15 AF5   I2S2_SD       6           SDTI
 * 25/PB12 AF5   I2S2_WS       7           LRCK
 */

//////////////////////////////////////////////////////////////////////////////

#define NUM_SAMPLES 4096

typedef struct {
  int32_t buffer[NUM_SAMPLES];
  I2S_HandleTypeDef i2s_handle;
} AudioState;

void init_i2s(AudioState* as) {
  GPIO_InitTypeDef GPIO_InitStruct;

  // Initialize MCK
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
  I2S_HandleTypeDef* const h = &as->i2s_handle;
  h->Instance = SPI2;
  h->Init.Mode = I2S_MODE_MASTER_TX;
  h->Init.Standard = I2S_STANDARD_PHILIPS;
  h->Init.DataFormat = I2S_DATAFORMAT_24B;
  h->Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
  h->Init.AudioFreq = I2S_AUDIOFREQ_48K;
  h->Init.CPOL = I2S_CPOL_LOW;
  h->Init.ClockSource = I2S_CLOCK_SYSCLK;

  /* Disable I2S block */
  __HAL_I2S_DISABLE(h);

  if (HAL_I2S_Init(h) != HAL_OK) {
    __builtin_trap();
  }
}

uint32_t htoi2s(uint32_t v) {
  // uint32_t b3 = (v>>24) & 0xff;
  uint32_t b2 = (v >> 16) & 0xff;
  uint32_t b1 = (v >> 8) & 0xff;
  uint32_t b0 = (v)&0xff;
  return (b0 << 24) | (b2 << 8) | b1;
}

void init_samples(AudioState* as) {
#if 0
  int sample = 0;
  for (int i = 0; i < (NUM_SAMPLES / (4* 2)); i++) {
    as->buffer[sample++] = htoi2s(0x00000033);
    as->buffer[sample++] = htoi2s(0xaabbccdd);
    as->buffer[sample++] = htoi2s(0x00005500);
    as->buffer[sample++] = htoi2s(0xaabbccdd);
    as->buffer[sample++] = htoi2s(0x00770000);
    as->buffer[sample++] = htoi2s(0xaabbccdd);
    as->buffer[sample++] = htoi2s(0x99000000);
    as->buffer[sample++] = htoi2s(0xaabbccdd);
  }
#endif
#if 0
  for (int i = 0; i < NUM_SAMPLES/3; i++) {
    as->buffer[i] = 0;
  }
  for (int i = NUM_SAMPLES/3; i < 2*NUM_SAMPLES/3; i++) {
    as->buffer[i] = 0x88888888;
  }
  for (int i = 2*NUM_SAMPLES/3; i < NUM_SAMPLES; i++) {
    as->buffer[i] = 0xFFFFFFFF;
  }
#endif
  int sample = 0;
  float scalefactor = (1 << 19);
  for (int i = 0; i < NUM_SAMPLES / 2; i++) {
    float sinewave =
        (1.0 + sin((3.1415926535 * 2 * i * 32) / (NUM_SAMPLES / 2)));
    int32_t intsinewave = (sinewave * scalefactor) - scalefactor;
    uint32_t value = htoi2s(intsinewave);
    as->buffer[sample++] = value;
    as->buffer[sample++] = value;
  }
}

int main() {
  hal_init();
  init_clock(10000, TIMER1);

#ifdef LOG_TO_SERIAL
  HalUart uart;
  hal_uart_init(&uart, 115200, true, /* uart_id= */ 0);
  LOG("Log output running");
#endif

  AudioState as;
  memset(&as, 0, sizeof(as));
  init_i2s(&as);
  init_samples(&as);

  int num = 0;
  while (true) {
    num++;
    // LOG("iteration %d", num);
    if (HAL_I2S_Transmit(&as.i2s_handle, (uint16_t*)as.buffer, NUM_SAMPLES,
                         HAL_MAX_DELAY) != HAL_OK) {
      __builtin_trap();
    }
#if 0
    for (volatile int i = 0; i < 20000; i++) {
    }
#endif
  }

  cpumon_main_loop();

  return 0;
}
