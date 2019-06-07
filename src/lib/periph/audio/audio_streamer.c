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

#include "periph/audio/audio_streamer.h"

static void fill_buffer_cb(void* user_data, int16_t* buffer_to_fill);
static void audio_done_cb(void* user_data);

void init_audio_streamer(AudioStreamer* as) {
  // Front half of fat initialization? :v)
  memset(as, 0, sizeof(*as));

  // Set up the i2s driver.
  as->i2s = i2s_init(SAMPLE_BUF_COUNT, 50000, as, fill_buffer_cb, audio_done_cb,
                     as->i2s_storage, sizeof(as->i2s_storage));

  // mount the FAT filesystem from the SD card.
  if (f_mount(&as->fatfs, "", 0) != FR_OK) {
    LOG("can't mount");
    __builtin_trap();
  }

  as->fp_valid = false;
  as->playing = false;
  as->mlvolume = 0;  // loud
}

static void fill_buffer_cb(void* user_data, int16_t* buffer_to_fill) {
  AudioStreamer* as = (AudioStreamer*)user_data;
  if (!as->fp_valid) {
    i2s_buf_filled(as->i2s, buffer_to_fill, 0);
    return;
  }
  UINT bytes_read;
  int retval =
      f_read(&as->fp, buffer_to_fill, SAMPLE_BUF_COUNT * 2, &bytes_read);
  if (retval != FR_OK) {
    LOG("read error reading fp: %d", retval);
    i2s_buf_filled(as->i2s, buffer_to_fill, 0);
    return;
  }
  // TODO apply volume adjustment here!
  i2s_buf_filled(as->i2s, buffer_to_fill, bytes_read / 2);
}

static void as_maybe_close_fp(AudioStreamer* as) {
  if (as->fp_valid) {
    f_close(&as->fp);
    as->fp_valid = false;
  }
}

static void audio_done_cb(void* user_data) {
  AudioStreamer* as = (AudioStreamer*)user_data;
  as_maybe_close_fp(as);
  as->playing = false;

  schedule_now(as->client_done_cb, as->client_done_data);
}

r_bool as_play(AudioStreamer* as, const char* pathname,
               ActivationFuncPtr client_done_cb, void* client_done_data) {
  as->client_done_cb = client_done_cb;
  as->client_done_data = client_done_data;

  // close any previously open file
  as_maybe_close_fp(as);

  // open the requested file
  if (f_open(&as->fp, pathname, FA_OPEN_EXISTING | FA_READ) != FR_OK) {
    LOG("can't open %s", pathname);
    return false;
  }
  as->fp_valid = true;

  if (!as->playing) {
    i2s_start(as->i2s);
  }
  return true;
}

void as_set_volume(AudioStreamer* as, uint8_t mlvolume) {
  as->mlvolume = mlvolume;
}

void as_stop_streaming(AudioStreamer* as) { as_maybe_close_fp(as); }
