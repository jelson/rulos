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

#ifndef _audio_server_h
#define _audio_server_h

#include "rocket.h"
#include "network.h"
#include "network_ports.h"
#include "audio_streamer.h"
#include "sound.h"
#include "audio_request_message.h"


typedef struct s_audio_server {
	AudioStreamer audio_streamer;

	uint8_t arm_recv_msg_alloc[sizeof(Message)+sizeof(AudioRequestMessage)];
	RecvSlot arm_recvSlot;
	uint8_t avm_recv_msg_alloc[sizeof(Message)+sizeof(AudioVolumeMessage)];
	RecvSlot avm_recvSlot;
	uint8_t mcm_recv_msg_alloc[sizeof(Message)+sizeof(MusicControlMessage)];
	RecvSlot mcm_recvSlot;

	SoundCmd skip_cmd;
	SoundCmd loop_cmd;

	r_bool index_ready;
	AuIndexRec magic;
	AuIndexRec index[sound_num_tokens];
	//AuIndexRec index[2];

	r_bool music_random_seeded;
	uint8_t cur_music_token; 	// ranges 0..num_music_tokens-1
	uint8_t num_music_tokens;	// derived from index
	uint8_t music_token_offset;	// offset into index where music starts.

	SDCard *borrowed_sdc;
} AudioServer;

void init_audio_server(AudioServer *as, Network *network, uint8_t timer_id);
void _aserv_skip_to_clip(AudioServer *aserv, SoundCmd cur_cmd, SoundCmd loop_cmd);

// visibility for debugging
void _aserv_fetch_start(AudioServer *as);

#endif // _audio_server_h
