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

#include "core/network.h"
#include "core/network_ports.h"
#include "periph/audio/audio_request_message.h"
#include "periph/audio/audio_streamer.h"
#include "periph/audio/sound.h"

typedef struct {
  SoundCmd skip_cmd;
  SoundCmd loop_cmd;
  uint8_t mlvolume;
} AudioEffectsStream;

typedef struct s_audio_server {
  AudioStreamer audio_streamer;

  uint8_t
      arm_recv_ring_alloc[RECEIVE_RING_SIZE(1, sizeof(AudioRequestMessage))];
  AppReceiver arm_app_receiver;
  uint8_t avm_recv_ring_alloc[RECEIVE_RING_SIZE(1, sizeof(AudioVolumeMessage))];
  AppReceiver avm_app_receiver;
  uint8_t
      mcm_recv_ring_alloc[RECEIVE_RING_SIZE(1, sizeof(MusicControlMessage))];
  AppReceiver mcm_app_receiver;

  r_bool index_ready;
  AuIndexRec magic;
  AuIndexRec index[sound_num_tokens];
  // AuIndexRec index[2];

  AudioEffectsStream audio_stream[AUDIO_NUM_STREAMS];
  int8_t active_stream;

  r_bool music_random_seeded;
  uint8_t num_music_tokens;   // derived from index
  uint8_t music_first_token;  // offset into index where music starts.
  uint8_t music_offset;       // offset past first_token of last thing we played

  SDCard *borrowed_sdc;
} AudioServer;

void init_audio_server(AudioServer *as, Network *network, uint8_t timer_id);

// visibility for debugging
void aserv_fetch_start(AudioServer *as);
void aserv_dbg_play(AudioServer *aserv, SoundToken skip, SoundToken loop);
