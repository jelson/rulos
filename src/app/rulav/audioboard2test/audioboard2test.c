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

// Test program used while developing code for audioboard2. Now integrated
// into src/app/audioboard.

#include "core/rulos.h"
#include "periph/fatfs/ff.h"
#include "periph/i2s/i2s.h"

// for network test
#include "core/network.h"
#include "core/network_ports.h"
#include "periph/input_controller/input_controller.h"
#include "periph/remote_keyboard/remote_keyboard.h"

//////////////////////////////////////////////////////////////////////////////

#define SAMPLE_BUF_COUNT 4096

//#define FAT_FILE "silence.bin"
//#define FAT_FILE "kool50.raw"
//#define FAT_FILE "chill50.raw"
#define FAT_FILE "music/funky.raw"

typedef struct {
  int num_buffers_filled;
  uint8_t i2s_storage[I2S_STATE_SIZE(SAMPLE_BUF_COUNT)];
  i2s_t* i2s;
  int16_t samplebuffer[SAMPLE_BUF_COUNT];
  FATFS fatfs;
  FIL fp;
} AudioState;

static void recv_keystroke(struct s_input_injector_ifc* iii, Keystroke k) {
  LOG("app got keystroke %c", k.key);
}

static void start_audio(void* user_data) {
  LOG("starting audio");
  AudioState* as = (AudioState*)user_data;
#ifdef FAT_FILE
  if (f_open(&as->fp, FAT_FILE, FA_OPEN_EXISTING | FA_READ) != FR_OK) {
    LOG("can't open %s", FAT_FILE);
    __builtin_trap();
  }
#else
  as->num_buffers_filled = 0;
#endif

  i2s_start(as->i2s);
}

static void fill_buffer_cb(void* user_data, int16_t* buffer_to_fill) {
  AudioState* as = (AudioState*)user_data;
#ifdef FAT_FILE
  UINT bytes_read;
  int retval =
      f_read(&as->fp, buffer_to_fill, SAMPLE_BUF_COUNT * 2, &bytes_read);
  if (retval != FR_OK) {
    LOG("read error reading from %s: %d", FAT_FILE, retval);
    __builtin_trap();
  }
  i2s_buf_filled(as->i2s, buffer_to_fill, bytes_read / 2);
#else
  if (++as->num_buffers_filled == 30) {
    i2s_buf_filled(as->i2s, buffer_to_fill, 0);
  } else {
    memcpy(buffer_to_fill, as->samplebuffer, sizeof(as->samplebuffer));
    i2s_buf_filled(as->i2s, buffer_to_fill, SAMPLE_BUF_COUNT);
  }
#endif
}

static void audio_done_cb(void* user_data) {
  LOG("audio done");

#ifdef FAT_FILE
  AudioState* as = (AudioState*)user_data;
  f_close(&as->fp);
#endif

  schedule_us(2000000, start_audio, user_data);
}

void init_samples(AudioState* as) {
  int sample = 0;
  // float scalefactor = (1 << 14);
  float scalefactor = (1 << 13);
  for (int i = 0; i < SAMPLE_BUF_COUNT / 2; i++) {
    float sinewave =
        (sinf(((float)3.14159 * 2 * i * 32) / (SAMPLE_BUF_COUNT / 2)));
    int32_t intsinewave = sinewave * scalefactor;
    as->samplebuffer[sample++] = intsinewave;
    as->samplebuffer[sample++] = intsinewave;
  }
}

#include "core/stats.h"

int main() {
  volatile uint16_t testnum = 0xcc33;
  testnum++;

  hal_init();
  init_clock(10000, TIMER1);

#ifdef LOG_TO_SERIAL
  HalUart uart;
  hal_uart_init(&uart, 115200, true, /* uart_id= */ 0);
  LOG("Log output running");
#endif

#if 0
  MinMaxMean_t mmm;
  minmax_init(&mmm);
  volatile Time now = precise_clock_time_us();
  for (int i = 0; i < 1000; i++) {
    for (volatile int j = 0; j < 10000; j++) {
    }
    volatile Time later = precise_clock_time_us();
    volatile int elapsed = later - now;
    assert(elapsed >= 0);
    minmax_add_sample(&mmm, elapsed);
    now = later;
  }
  minmax_log(&mmm, "test");
  LOG("done");
#endif

  AudioState as;
  memset(&as, 0, sizeof(as));

#ifdef FAT_FILE
  if (f_mount(&as.fatfs, "", 0) != FR_OK) {
    LOG("can't mount");
    __builtin_trap();
  }
#else
  init_samples(&as);
#endif

  as.i2s = i2s_init(SAMPLE_BUF_COUNT, 50000, &as, fill_buffer_cb, audio_done_cb,
                    as.i2s_storage, sizeof(as.i2s_storage));
  schedule_now((ActivationFuncPtr)start_audio, &as);

  // network receiver test
  RemoteKeyboardRecv rk;
  Network net;
  InputInjectorIfc iii;
  iii.func = &recv_keystroke;
  init_twi_network(&net, 200, ROCKET_ADDR);
  init_remote_keyboard_recv(&rk, &net, &iii, REMOTE_KEYBOARD_PORT);

  cpumon_main_loop();

  return 0;
}
