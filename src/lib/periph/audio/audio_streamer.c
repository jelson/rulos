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

#include "periph/audio/sound.h"

static void fill_buffer_cb(void *user_data, int16_t *buffer_to_fill);
static void audio_done_cb(void *user_data);
static void adjust_volume(int16_t *buffer, int len_samples, int volume_level);

void test_volume() {
  int16_t buf[2];
  for (int volume_level = VOL_MIN; volume_level <= VOL_MAX; volume_level++) {
    buf[0] = 1000;
    buf[1] = 10000;
    adjust_volume(buf, 2, volume_level);
    LOG("volume %d buf %d %d", volume_level, buf[0], buf[1]);
  }
}

void init_audio_streamer(AudioStreamer *as) {
  // test_volume();

  // Front half of fat initialization? :v)
  memset(as, 0, sizeof(*as));

  // Set up the i2s driver.
  as->i2s = i2s_init(SAMPLE_BUF_COUNT, 50000, as, fill_buffer_cb, audio_done_cb,
                     as->i2s_storage, sizeof(as->i2s_storage));

  as->fp_valid = false;
  as->playing = false;
  as->volume = 27;  // fairly loud; range VOL_MIN--VOL_MAX
}

// Upcall from the I2S driver telling us it's time to give it the next audio
// buffer.
static void fill_buffer_cb(void *user_data, int16_t *buffer_to_fill) {
  AudioStreamer *as = (AudioStreamer *)user_data;
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
  adjust_volume(buffer_to_fill, SAMPLE_BUF_COUNT, as->volume);
  i2s_buf_filled(as->i2s, buffer_to_fill, bytes_read / 2);
}

static void adjust_volume(int16_t *buffer, int len_samples, int volume_level) {
  int neg_vol = VOL_MAX - volume_level;
  int shift_ = neg_vol >> 1;
  int mult = 1;
  if (neg_vol & 1) {
    shift_ += 4;
    mult = 11;
  }
  for (int i = 0; i < len_samples; i++) {
    int sample = buffer[i];
    sample = sample * mult >> shift_;
    buffer[i] = sample;
  }
}

static void as_maybe_close_fp(AudioStreamer *as) {
  if (as->fp_valid) {
    f_close(&as->fp);
    as->fp_valid = false;
  }
}

// Upcall from the I2S driver telling us it has finished playing the last buffer
// we gave it.
static void audio_done_cb(void *user_data) {
  AudioStreamer *as = (AudioStreamer *)user_data;
  as_maybe_close_fp(as);
  as->playing = false;

  schedule_now(as->client_done_cb, as->client_done_data);
}

bool as_play(AudioStreamer *as, const char *pathname,
             ActivationFuncPtr client_done_cb, void *client_done_data) {
  as->client_done_cb = client_done_cb;
  as->client_done_data = client_done_data;

  // close any previously open file
  as_maybe_close_fp(as);

  // open the requested file
  if (f_open(&as->fp, pathname, FA_OPEN_EXISTING | FA_READ) != FR_OK) {
    LOG("can't open %s", pathname);
    return false;
  }
  // LOG("XXX opened %s", pathname);
  as->fp_valid = true;

  if (!as->playing) {
    i2s_start(as->i2s);
    as->playing = true;
  }
  return true;
}

void as_set_volume(AudioStreamer *as, uint8_t volume) { as->volume = volume; }

void as_stop_streaming(AudioStreamer *as) { as_maybe_close_fp(as); }
