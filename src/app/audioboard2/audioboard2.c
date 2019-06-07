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
  uint8_t i2s_storage[I2S_STATE_SIZE(SAMPLE_BUF_COUNT)];
  i2s_t* i2s;
  FATFS fatfs;
  FIL fp;
  bool file_open;
} AudioState;

static void fill_buffer_cb(void* user_data, int16_t* buffer_to_fill) {
  AudioState* as = (AudioState*)user_data;
  UINT bytes_read;
  int retval =
      f_read(&as->fp, buffer_to_fill, SAMPLE_BUF_COUNT * 2, &bytes_read);
  if (retval != FR_OK) {
    LOG("read error reading from %s: %d", FAT_FILE, retval);
    __builtin_trap();
  }
  i2s_buf_filled(as->i2s, buffer_to_fill, bytes_read / 2);
}

static void audio_done_cb(void* user_data) {
  LOG("audio done");

  AudioState* as = (AudioState*)user_data;
  f_close(&as->fp);
  as->file_open = false;

  schedule_us(2000000, start_audio, user_data);
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
  
  as.i2s = i2s_init(SAMPLE_BUF_COUNT, 50000, /*user ptr*/ &as,
        fill_buffer_cb, audio_done_cb, as.i2s_storage, sizeof(as.i2s_storage));
  
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

  Network net;
  // TODO why is the network speed (200) a hard-coded magic constant!?
  init_twi_network(&net, 200, AUDIO_ADDR);

  schedule_us(10000, start_read, NULL);
  cpumon_main_loop();

  return 0;
}
