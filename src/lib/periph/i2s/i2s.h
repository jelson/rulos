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
#include "core/stats.h"

// Assumptions:
//
// * The only format currently supported is 16-bit, signed, little-endian.
//
// * There are exactly two buffers (one playing, one filling), though the caller
//   can vary the size of those buffers.
#define I2S_BUFSIZE_BYTES(samples_per_buf) (samples_per_buf * sizeof(uint16_t))

#define I2S_STATE_SIZE(samples_per_buf) \
  (sizeof(i2s_t) + (I2S_BUFSIZE_BYTES(samples_per_buf) * 2))

typedef enum {
  EMPTY,
  FILLING,
  FULL,
  PLAYING,
} i2s_buf_state_t;

// Upcall from library to caller indicating we want another sample
// buffer filled. Format is 16-bit, signed, little-endian.
typedef void (*i2s_fill_buffer_cb_t)(void* user_data, uint16_t* buf);

// Upcall from library to caller indicating audio play has completed.
typedef void (*i2s_audio_done_cb_t)(void* user_data);

typedef struct {
  // input data from the caller
  uint16_t samples_per_buf;
  uint32_t sample_rate;
  void* user_data;
  i2s_fill_buffer_cb_t fill_buffer_cb;
  i2s_audio_done_cb_t audio_done_cb;

  // internal state
  i2s_buf_state_t buf_state[2];
  uint16_t samples_in_buf[2];
  Time last_play_done_time;
  Time buf_fill_start_time;
  MinMaxMean_t buf_play_time_mmm;
  MinMaxMean_t buf_load_time_mmm;

  // Data for both buffers, required to be bufsize *
  uint8_t bufdata[0];
} i2s_t;

// Initialize a new I2S peripheral. We double-buffer to provide a
// continuous flow of data out to the I2S hardware. The caller can
// decide how deep these buffers should be and must provide the
// storage for them (plus the library's other internal state). For
// buffers of depth `samples_per_buf`, the total size of rawbuf
// provided must be I2S_STATE_SIZE(samples_per_buf). 16-bit samples
// are assumed.
i2s_t* i2s_init(uint16_t samples_per_buf, uint32_t sample_rate, void* user_data,
                i2s_fill_buffer_cb_t fill_buffer,
                i2s_audio_done_cb_t audio_done, uint8_t* rawbuf,
                uint16_t allocated_size);

// Start playing audio. Library will call fill_buffer (provided to
// init) every time a new buffer needs to be filled, and audio_done
// (provided to init) when the audio has finished playing.
void i2s_start(i2s_t* i2s);

// Downcall from app into library that should be called to indicate a
// buf-fill call requesteded by i2s_fill_buffer_cb_t has been
// completed. If samples_filled == samples_per_buf, we are mid-stream
// and another i2s_fill_buffer_cb_t upcall will be made to request more
// data. If sample_filled is < samples_per_buf, this is assumed to be
// the last buffer and audio output will stop once this has been
// played. Length of 0 is legal.
void i2s_buf_filled(i2s_t* i2s, uint16_t* buf, uint16_t samples_filled);
