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

#include "periph/rocket/slow_boot.h"

void slowboot_update(SlowBoot *slowboot);

#define SB_ANIM_INTERVAL 100000
#define NO_SLOW_BOOT     0

void init_slow_boot(SlowBoot *slowboot, ScreenBlanker *screenblanker,
                    AudioClient *audioClient) {
#if NO_SLOW_BOOT
  return;
#endif

  slowboot->screenblanker = screenblanker;
  slowboot->audioClient = audioClient;
  slowboot->startTime = clock_time_us();
  screenblanker_setmode(slowboot->screenblanker, sb_black);

  for (int bi = 0; bi < SLOW_MAX_BUFFERS; bi++) {
    board_buffer_init(&slowboot->buffer[bi] DBG_BBUF_LABEL("slowboot"));
    board_buffer_push(&slowboot->buffer[bi], bi);
  }

  schedule_us(1, (ActivationFuncPtr)slowboot_update, slowboot);
}

void slowboot_update(SlowBoot *slowboot) {
  Time elapsedTime = clock_time_us() - slowboot->startTime;

  uint8_t step = elapsedTime / SB_ANIM_INTERVAL;

  if (step < 4) {
    goto exit;
  }
  step -= 4;
  if (step < 32) {
    for (int bi = 0; bi < SLOW_MAX_BUFFERS; bi++) {
      int dist = (step + ((bi * 181) % SLOW_MAX_BUFFERS)) / 5 - 2;
      // LOG("step %2d bi %2d dist %2d", step, bi, dist);
      if (dist < 0 || dist > 4) {
        continue;
      }
      memset(slowboot->buffer[bi].buffer, 0, NUM_DIGITS);
      slowboot->buffer[bi].buffer[dist] = SSB_SEG_e | SSB_SEG_f | SSB_SEG_g;
      slowboot->buffer[bi].buffer[NUM_DIGITS - 1 - dist] =
          SSB_SEG_b | SSB_SEG_c | SSB_SEG_g;
      int di;
      for (di = 0; di < dist; di++) {
        slowboot->buffer[bi].buffer[di] = 0xff;
        slowboot->buffer[bi].buffer[NUM_DIGITS - 1 - di] = 0xff;
      }
      board_buffer_draw(&slowboot->buffer[bi]);
    }
    goto exit;
  }
  step -= 32;
  if (step < 2) {
    for (int bi = 0; bi < SLOW_MAX_BUFFERS; bi++) {
      memset(slowboot->buffer[bi].buffer, 0, NUM_DIGITS);
    }
    goto exit;
  }
  step -= 2;
  if (step == 0) {
    ac_skip_to_clip(slowboot->audioClient, AUDIO_STREAM_BURST_EFFECTS,
                    sound_quindar_key_down, sound_silence);
  }
  step /= 4;
  if (step < 5) {
    for (int bi = 0; bi < SLOW_MAX_BUFFERS; bi++) {
      memset(slowboot->buffer[bi].buffer, 0, NUM_DIGITS);
    }
#if SLOW_MAX_BUFFERS > 6
    switch (step) {
      case 4:
        ascii_to_bitmap_str(slowboot->buffer[6].buffer, NUM_DIGITS, " Vehicle");
      case 3:
        ascii_to_bitmap_str(slowboot->buffer[5].buffer, NUM_DIGITS, "Altitude");
      case 2:
        ascii_to_bitmap_str(slowboot->buffer[4].buffer, NUM_DIGITS, "UltraLow");
      case 1:
        ascii_to_bitmap_str(slowboot->buffer[4].buffer, NUM_DIGITS, "Ultra");
      case 0:
        ascii_to_bitmap_str(slowboot->buffer[3].buffer, NUM_DIGITS, "Ravenna ");
    }
#endif
    for (int bi = 0; bi < SLOW_MAX_BUFFERS; bi++) {
      board_buffer_draw(&slowboot->buffer[bi]);
    }
    goto exit;
  }
  step -= 5;
  if (step < 3) {
    goto exit;
  }

  // all done. Tear down display, and never schedule me again.
#if !BORROW_SCREENBLANKER_BUFS
  for (int bi = 0; bi < SLOW_MAX_BUFFERS; bi++) {
    board_buffer_pop(&slowboot->buffer[bi]);
  }
#endif  // BORROW_SCREENBLANKER_BUFS
  screenblanker_setmode(slowboot->screenblanker, sb_inactive);
  return;

exit:
  schedule_us(SB_ANIM_INTERVAL, (ActivationFuncPtr)slowboot_update, slowboot);
}
