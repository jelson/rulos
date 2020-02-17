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

// Each stream plays the "skip_effect_id" until it's done, then plays the
// "loop_effect_id" forever.
typedef struct {
  SoundEffectId skip_effect_id;
  SoundEffectId loop_effect_id;
  uint8_t mlvolume;
} AudioEffectsStream;

typedef struct s_audio_server {
  uint8_t
      arm_recv_ring_alloc[RECEIVE_RING_SIZE(1, sizeof(AudioRequestMessage))];
  AppReceiver arm_app_receiver;
  uint8_t avm_recv_ring_alloc[RECEIVE_RING_SIZE(1, sizeof(AudioVolumeMessage))];
  AppReceiver avm_app_receiver;
  uint8_t
      mcm_recv_ring_alloc[RECEIVE_RING_SIZE(1, sizeof(MusicControlMessage))];
  AppReceiver mcm_app_receiver;

  AudioStreamer audio_streamer;

  AudioEffectsStream audio_stream[AUDIO_NUM_STREAMS];
  int8_t active_stream;

  FATFS fatfs;   // Filesystem state for (globally-shared) FAT fs.

  int16_t music_file_count; // How many music files we found on the filesystem
  int16_t music_offset;     // Which one we should play next.
  r_bool music_random_seeded;   // On every boot, begin disco at a random song
} AudioServer;

void init_audio_server(AudioServer *as, Network *network, uint8_t timer_id);
