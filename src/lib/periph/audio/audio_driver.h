/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
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
#include "periph/ring_buffer/rocket_ring_buffer.h"
#include "periph/rocket/sound.h"
//#include "graveyard/spiflash.h"

#define NUM_COMPOSITE_STREAMS 1
#if 0
#define AUDIO_COMPOSITE_STREAM_BURST_EFFECTS 0
#define AUDIO_COMPOSITE_STREAM_CONTINUOUS_EFFECTS 1
#define AUDIO_COMPOSITE_STREAM_COUNTDOWN 2
#endif

#define AUDIO_RING_BUFFER_SIZE 50
// 6.25ms

typedef struct s_audio_stream {
  SoundToken cur_token;
  uint32_t cur_offset;
  SoundToken loop_token;

  uint8_t ring_buffer_storage[sizeof(RingBuffer) + 1 + AUDIO_RING_BUFFER_SIZE];
  RingBuffer *ring;
} AudioStream;

typedef struct s_audio_driver {
  // Streams tell what addresses to fetch, and carry ring buffers to
  // hold fetched data for compositing
  AudioStream stream[NUM_COMPOSITE_STREAMS];

  // Data comes from here
  // SPIFlash spif;

  // SPI read requests live here; we pass pointers in to SPIFlash.
  // SPIBuffer spibspace[2];
  // We toggle back and forth between spare spaces with this index
  uint8_t next_spib;

  // Points to hardware output buffer, which we fill from the compositing step
  RingBuffer *output_buffer;

#define TEST_OUTPUT_ONLY 0
#if TEST_OUTPUT_ONLY
  uint8_t test_output_val;
#endif  // TEST_OUTPUT_ONLY

} AudioDriver;

void init_audio_driver(AudioDriver *ad);

void ad_skip_to_clip(AudioDriver *ad, uint8_t stream_idx, SoundToken cur_token,
                     SoundToken loop_token);
// Change immediately to cur_token, then play loop_token in a loop

void ad_queue_loop_clip(AudioDriver *ad, uint8_t stream_idx,
                        SoundToken loop_token);
// After current token finishes, start looping loop_token
