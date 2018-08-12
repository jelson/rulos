/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

#pragma once

#include "core/network.h"
#include "core/network_ports.h"
#include "core/rulos.h"
#include "periph/audio/audio_request_message.h"
#include "periph/audio/audio_streamer.h"
#include "periph/rocket/ambient_noise.h"

typedef struct s_audio_client {
	Network *network;
	uint8_t arm_send_msg_alloc[sizeof(Message)+sizeof(AudioRequestMessage)];
	SendSlot arm_send_slot;
	uint8_t avm_send_msg_alloc[sizeof(Message)+sizeof(AudioVolumeMessage)];
	SendSlot avm_send_slot;
	uint8_t mcm_send_msg_alloc[sizeof(Message)+sizeof(MusicControlMessage)];
	SendSlot mcm_send_slot;
	uint8_t cached_music_volume;
	AmbientNoise ambient_noise;
} AudioClient;

void init_audio_client(AudioClient *ac, Network *network);
r_bool ac_skip_to_clip(AudioClient *ac, uint8_t stream_idx, SoundToken cur_token, SoundToken loop_token);
//r_bool ac_queue_loop_clip(AudioClient *ac, uint8_t stream_idx, SoundToken loop_token);
r_bool ac_change_volume(AudioClient *ac, uint8_t stream_id, uint8_t mlvolume);
r_bool ac_send_music_control(AudioClient *ac, int8_t advance);
