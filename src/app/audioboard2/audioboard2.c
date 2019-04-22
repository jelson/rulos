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

#include "core/rulos.h"
#include "periph/fatfs/ff.h"

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
  int16_t buffer[NUM_SAMPLES];
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
  h->Init.DataFormat = I2S_DATAFORMAT_16B;
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
  // float scalefactor = (1 << 14);
  float scalefactor = (1 << 13);
  for (int i = 0; i < NUM_SAMPLES / 2; i++) {
    float sinewave = (sinf(((float)3.14159 * 2 * i * 32) / (NUM_SAMPLES / 2)));
    int32_t intsinewave = sinewave * scalefactor;
    int16_t value = ((intsinewave >> 8) & 0xff) | ((intsinewave & 0xff) << 8);
    as->buffer[sample++] = value;
    as->buffer[sample++] = value;
  }
}

FATFS fatfs;
FIL f;

static void start_read(void* data) {
  if (f_mount(&fatfs, "", 0) != FR_OK) {
    LOG("can't mount");
    __builtin_trap();
  }

  const char filename[] = "audio.wav";

  LOG("card ok!");

  if (f_open(&f, filename, FA_OPEN_EXISTING | FA_READ) != FR_OK) {
    LOG("can't open %s", filename);
    __builtin_trap();
  }
  char buf[1024];
  UINT bytes_read;
  FRESULT retval = f_read(&f, buf, sizeof(buf), &bytes_read);
  if (retval != FR_OK) {
    LOG("read error %d", retval);
    __builtin_trap();
  }
  f_close(&f);
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

#if 1
  int num = 0;
  while (true) {
    num++;
    // LOG("iteration %d", num);
    if (HAL_I2S_Transmit(&as.i2s_handle, (uint16_t*)as.buffer, NUM_SAMPLES,
                         HAL_MAX_DELAY) != HAL_OK) {
      __builtin_trap();
    }
  }
#endif

  schedule_us(10000, start_read, NULL);
  cpumon_main_loop();

  return 0;
}
