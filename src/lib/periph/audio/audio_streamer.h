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

#pragma once

#include "core/rulos.h"
#include "periph/fatfs/ff.h"
#include "periph/i2s/i2s.h"

#define SAMPLE_BUF_COUNT 4096

typedef struct s_AudioStreamer {
  // Storage used by I2S to DMA to DAC
  uint8_t i2s_storage[I2S_STATE_SIZE(SAMPLE_BUF_COUNT)];

#ifndef SIM
  // I2S driver to pump audio to DAC.
  i2s_t *i2s;
#endif // SIM

  // Filesystem
  FATFS fatfs;

  // Currently-playing file.
  FIL fp;

  // Whether fp is valid, so we know whether we need to close fp.
  bool fp_valid;

  // i2s is playing, and owes us a callback.
  bool playing;

  uint8_t mlvolume;

  ActivationFuncPtr client_done_cb;
  void *client_done_data;
} AudioStreamer;

void init_audio_streamer(AudioStreamer *as);

// Play sample at filename
r_bool as_play(AudioStreamer *as, const char *pathname,
               ActivationFuncPtr client_done_cb, void *client_done_data);

// Adjust the volume multiplier. (mlvolume is neg log: 0 is shift off zero bits
// == max loud)
void as_set_volume(AudioStreamer *as, uint8_t mlvolume);

// Stop the ongoing streaming.
void as_stop_streaming(AudioStreamer *as);
