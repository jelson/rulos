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
#include "periph/audio/audio_request_message.h"

typedef struct s_audio_client {
  Network *network;
  uint8_t arm_send_msg_alloc[sizeof(WireMessage) + sizeof(AudioRequestMessage)];
  SendSlot arm_send_slot;
  uint8_t avm_send_msg_alloc[sizeof(WireMessage) + sizeof(AudioVolumeMessage)];
  SendSlot avm_send_slot;
  uint8_t mcm_send_msg_alloc[sizeof(WireMessage) + sizeof(MusicControlMessage)];
  SendSlot mcm_send_slot;

  uint8_t volume_for_stream[AUDIO_NUM_STREAMS];
} AudioClient;

void init_audio_client(AudioClient *ac, Network *network);

/*
 * Stop whatever is playing on stream_idx and skip to cur_effect_id. When that's
 * done playing, play loop_effect_id forever.
 */
bool ac_skip_to_clip(AudioClient *ac, uint8_t stream_idx,
                     SoundEffectId cur_effect_id, SoundEffectId loop_effect_id);

/* Configure volume of effect playing on stream_idx. */
bool ac_change_volume(AudioClient *ac, uint8_t stream_idx, uint8_t mlvolume);

/* Change what music is playing on AUDIO_STREAM_MUSIC. */
bool ac_send_music_control(AudioClient *ac, int8_t advance);
