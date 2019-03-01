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

#include "periph/audio/audio_client.h"

extern void syncdebug(uint8_t spaces, char f, uint16_t line);
#define SYNCDEBUG() syncdebug(0, 'U', __LINE__)
//#define SYNCDEBUG()	{}

void init_audio_client(AudioClient *ac, Network *network) {
  ac->network = network;

  ac->arm_send_slot.func = NULL;
  ac->arm_send_slot.wire_msg = (WireMessage *)ac->arm_send_msg_alloc;
  ac->arm_send_slot.sending = FALSE;

  ac->avm_send_slot.func = NULL;
  ac->avm_send_slot.wire_msg = (WireMessage *)ac->avm_send_msg_alloc;
  ac->avm_send_slot.sending = FALSE;

  ac->mcm_send_slot.func = NULL;
  ac->mcm_send_slot.wire_msg = (WireMessage *)ac->mcm_send_msg_alloc;
  ac->mcm_send_slot.sending = FALSE;

  ac->cached_music_volume = 2;  // should be immediately overwritten

  ambient_noise_init(&ac->ambient_noise, ac);
}

r_bool ac_skip_to_clip(AudioClient *ac, uint8_t stream_idx,
                       SoundToken cur_token, SoundToken loop_token) {
  if (ac->arm_send_slot.sending) {
    return FALSE;
  }

  ac->arm_send_slot.dest_addr = AUDIO_ADDR;
  ac->arm_send_slot.wire_msg->dest_port = AUDIO_PORT;
  ac->arm_send_slot.payload_len = sizeof(AudioRequestMessage);
  AudioRequestMessage *arm =
      (AudioRequestMessage *)&ac->arm_send_slot.wire_msg->data;
  arm->stream_id = stream_idx;
  arm->skip = TRUE;
  arm->skip_cmd.token = cur_token;
  arm->loop_cmd.token = loop_token;
  net_send_message(ac->network, &ac->arm_send_slot);

  return TRUE;
}

#if 0  // unused
r_bool ac_queue_loop_clip(AudioClient *ac, uint8_t stream_idx, SoundToken loop_token)
{
	if (ac->sendSlot.sending)
	{
		return FALSE;
	}

	ac->sendSlot.dest_addr = AUDIO_ADDR;
	ac->sendSlot.msg->dest_port = AUDIO_PORT;
	ac->sendSlot.msg->payload_len = sizeof(AudioRequestMessage);
	AudioRequestMessage *arm = (AudioRequestMessage *) &ac->sendSlot.msg->data;
	arm->skip = FALSE;
	arm->skip_cmd.token = (SoundToken) -1;
	arm->loop_cmd.token = loop_token;
	net_send_message(ac->network, &ac->sendSlot);

	return TRUE;
}
#endif

r_bool ac_change_volume(AudioClient *ac, uint8_t stream_id, uint8_t mlvolume) {
  if (ac->avm_send_slot.sending) {
    return FALSE;
  }

  ac->avm_send_slot.dest_addr = AUDIO_ADDR;
  ac->avm_send_slot.wire_msg->dest_port = SET_VOLUME_PORT;
  ac->avm_send_slot.payload_len = sizeof(AudioVolumeMessage);
  AudioVolumeMessage *avm =
      (AudioVolumeMessage *)&ac->avm_send_slot.wire_msg->data;
  avm->stream_id = stream_id;
  avm->mlvolume = mlvolume;
  net_send_message(ac->network, &ac->avm_send_slot);

  return TRUE;
}

r_bool ac_send_music_control(AudioClient *ac, int8_t advance) {
  if (ac->mcm_send_slot.sending) {
    return FALSE;
  }

  ac->mcm_send_slot.dest_addr = AUDIO_ADDR;
  ac->mcm_send_slot.wire_msg->dest_port = MUSIC_CONTROL_PORT;
  ac->mcm_send_slot.payload_len = sizeof(MusicControlMessage);
  MusicControlMessage *mcm =
      (MusicControlMessage *)&ac->mcm_send_slot.wire_msg->data;
  mcm->advance = advance;
  net_send_message(ac->network, &ac->mcm_send_slot);

  return TRUE;
}
