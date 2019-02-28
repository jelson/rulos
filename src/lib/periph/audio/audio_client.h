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

#include "core/network.h"
#include "core/network_ports.h"
#include "core/rulos.h"
#include "periph/audio/audio_request_message.h"
#include "periph/audio/audio_streamer.h"
#include "periph/rocket/ambient_noise.h"

typedef struct s_audio_client {
  Network *network;
  uint8_t arm_send_msg_alloc[sizeof(WireMessage) + sizeof(AudioRequestMessage)];
  SendSlot arm_send_slot;
  uint8_t avm_send_msg_alloc[sizeof(WireMessage) + sizeof(AudioVolumeMessage)];
  SendSlot avm_send_slot;
  uint8_t mcm_send_msg_alloc[sizeof(WireMessage) + sizeof(MusicControlMessage)];
  SendSlot mcm_send_slot;
  uint8_t cached_music_volume;
  AmbientNoise ambient_noise;
} AudioClient;

void init_audio_client(AudioClient *ac, Network *network);
r_bool ac_skip_to_clip(AudioClient *ac, uint8_t stream_idx,
                       SoundToken cur_token, SoundToken loop_token);
// r_bool ac_queue_loop_clip(AudioClient *ac, uint8_t stream_idx, SoundToken
// loop_token);
r_bool ac_change_volume(AudioClient *ac, uint8_t stream_id, uint8_t mlvolume);
r_bool ac_send_music_control(AudioClient *ac, int8_t advance);
