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

#include "periph/i2s/i2s.h"
#include "periph/i2s/hal_i2s.h"

#include "core/rulos.h"

#define BUF_ADDR(i2s, buf_num)           \
  ((uint16_t*)(&(i2s)->bufdata[buf_num * \
                               I2S_BUFSIZE_BYTES((i2s)->samples_per_buf)]))

static void i2s_request_buffer_fill_trampoline(void* data) {
  i2s_t* i2s = (i2s_t*)data;
  uint8_t idx_to_fill = 0;

  if (i2s->buf_state[0] == FILLING) {
    idx_to_fill = 0;
  } else if (i2s->buf_state[1] == FILLING) {
    idx_to_fill = 1;
  } else {
    assert(FALSE);
  }

  i2s->fill_buffer_cb(i2s->user_data, BUF_ADDR(i2s, idx_to_fill));
}

// Ask the layer above to fill a buffer with audio data and give us a down-call
// when it's finished.
static void i2s_request_buffer_fill(i2s_t* i2s, uint8_t buf_num) {
  assert(i2s->buf_state[buf_num] == EMPTY);
  i2s->buf_state[buf_num] = FILLING;
  schedule_now(i2s_request_buffer_fill_trampoline, i2s);
}

// Called whenever a buffer has just started playing. This can happen either
// when we just began playing audio or when the HAL informed us that it has
// swapped over and begun playing the other buffer.
static void i2s_buf_is_playing(i2s_t* i2s, uint8_t play_idx,
                               uint8_t other_idx) {
  assert(i2s->buf_state[play_idx] == FULL);

  i2s->buf_state[play_idx] = PLAYING;

  // Only request the next buffer if the prior one was full. If the prior one
  // was short, that indicates we'd reached end-of-stream.
  if (i2s->samples_in_buf[play_idx] == i2s->samples_per_buf) {
    i2s_request_buffer_fill(i2s, other_idx);
  }
}

// Downcall when a buf fill has been completed by the upper layer.
static void i2s_buf_filled_internal(i2s_t* i2s, uint16_t samples_filled,
                                    uint8_t just_filled_idx,
                                    uint8_t other_idx) {
  assert(i2s->buf_state[just_filled_idx] == FILLING);
  i2s->samples_in_buf[just_filled_idx] = samples_filled;
  hal_i2s_condition_buffer(BUF_ADDR(i2s, just_filled_idx), samples_filled);
  i2s->buf_state[just_filled_idx] = FULL;

  // The HAL will play the entire buffer, so we must zero out the end of the
  // buffer if the layer above didn't fill the entire thing with data.
  if (samples_filled < i2s->samples_per_buf) {
    memset(BUF_ADDR(i2s, just_filled_idx) + samples_filled, 0,
           sizeof(uint16_t) * (i2s->samples_per_buf - samples_filled));
  }

  switch (i2s->buf_state[other_idx]) {
    case EMPTY:
      // We are just starting up or resuming after a buffer underrun. Start the
      // just-filled I2S data flowing to hardware.
      //
      // Note, we can only start playing from the beginning of the double-buffer
      // so if we're trying to start from the middle move the data to the
      // beginning.
      if (just_filled_idx == 1) {
        memcpy(BUF_ADDR(i2s, 0), BUF_ADDR(i2s, 1),
               sizeof(uint16_t) * i2s->samples_per_buf);
        just_filled_idx = 0;
        other_idx = 1;
      }

      assert(just_filled_idx == 0);
      hal_i2s_start(BUF_ADDR(i2s, just_filled_idx), i2s->samples_per_buf);
      i2s_buf_is_playing(i2s, just_filled_idx, other_idx);
      break;

    case PLAYING:
      // We don't need to do anything other than remember that this buf is now
      // full so that the switch can be done when the hal calls into us at
      // interrupt time.
      break;

    default:
      assert(FALSE);
  }
}

// Downcall that indicates the buffer fill requested earlier has completed.
void i2s_buf_filled(i2s_t* i2s, uint16_t* buf, uint16_t samples_filled) {
  if (buf == BUF_ADDR(i2s, 0)) {
    i2s_buf_filled_internal(i2s, samples_filled, 0, 1);
  } else if (buf == BUF_ADDR(i2s, 1)) {
    i2s_buf_filled_internal(i2s, samples_filled, 1, 0);
  } else {
    assert(FALSE);
  }
}

// Called when the HAL informs us that an I2S buffer has just finished playing.
//
// WARNING: Called at interrupt time!
static void i2s_buf_played_cb_internal(i2s_t* i2s, uint8_t just_played_idx,
                                       uint8_t other_idx) {
  assert(i2s->buf_state[just_played_idx] == PLAYING);
  i2s->buf_state[just_played_idx] = EMPTY;

  switch (i2s->buf_state[other_idx]) {
    case FULL:
      // The other buffer was already full, as expected in normal operation.
      // Record the fact that the buffer is playing and request a buffer fill
      // for the buffer that just finished.
      i2s_buf_is_playing(i2s, other_idx, just_played_idx);
      break;

    case FILLING:
      // Buffer underrun! Drat. Stop the audio. It'll be restarted when the
      // buffer fill completes.
      hal_i2s_stop();
      break;

    case EMPTY:
      // Done playing! Stop the audio output and call the done upcall in the
      // layer above.
      hal_i2s_stop();
      schedule_now(i2s->audio_done_cb, i2s->user_data);
      break;

    default:
      assert(FALSE);
      break;
  }
}

// WARNING: Called at interrupt time!
static void i2s_buf_played_cb(void* user_data, uint8_t just_played_idx) {
  i2s_t* i2s = (i2s_t*)user_data;
  if (just_played_idx == 0) {
    i2s_buf_played_cb_internal(i2s, 0, 1);
  } else if (just_played_idx == 1) {
    i2s_buf_played_cb_internal(i2s, 1, 0);
  } else {
    assert(FALSE);
  }
}

//// API implementation

i2s_t* i2s_init(uint16_t samples_per_buf, uint32_t sample_rate, void* user_data,
                i2s_fill_buffer_cb_t fill_buffer_cb,
                i2s_audio_done_cb_t audio_done_cb, uint8_t* rawbuf,
                uint16_t allocated_size) {
  assert(allocated_size == I2S_STATE_SIZE(samples_per_buf));
  i2s_t* i2s = (i2s_t*)rawbuf;

  // note: this only initializes the state, not the buffers.
  memset(i2s, 0, sizeof(i2s_t));

  // input parameters
  i2s->samples_per_buf = samples_per_buf;
  i2s->sample_rate = sample_rate;
  i2s->user_data = user_data;
  i2s->fill_buffer_cb = fill_buffer_cb;
  i2s->audio_done_cb = audio_done_cb;

  // internal state
  i2s->buf_state[0] = EMPTY;
  i2s->buf_state[1] = EMPTY;

  hal_i2s_init(sample_rate, i2s_buf_played_cb, i2s);

  return i2s;
}

void i2s_start(i2s_t* i2s) {
  assert(i2s->buf_state[0] == EMPTY);
  assert(i2s->buf_state[1] == EMPTY);

  // Start buffer 0 filling.
  i2s_request_buffer_fill(i2s, 0);
}
