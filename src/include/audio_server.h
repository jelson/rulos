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

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	uint8_t stream_idx;
	r_bool skip;
	SoundToken skip_token;
	SoundToken loop_token;
} AudioRequestMessage;

typedef struct s_audio_client {
	Network *network;
	uint8_t send_msg_alloc[sizeof(Message)+sizeof(AudioRequestMessage)];
	SendSlot sendSlot;
} AudioClient;

void init_audio_client(AudioClient *ac, Network *network);
r_bool ac_skip_to_clip(AudioClient *ac, uint8_t stream_idx, SoundToken cur_token, SoundToken loop_token);
r_bool ac_queue_loop_clip(AudioClient *ac, uint8_t stream_idx, SoundToken loop_token);

//////////////////////////////////////////////////////////////////////////////

struct s_audio_server;
typedef struct {
	Activation act;
	struct s_audio_server *aserv;
} AudioServerAct;

typedef struct s_audio_server {
	Activation retry;

	AudioStreamer audio_streamer;

	uint8_t recv_msg_alloc[sizeof(Message)+sizeof(AudioRequestMessage)];
	RecvSlot recvSlot;

	AudioServerAct fetch_start;
	AudioServerAct fetch_complete;
	AudioServerAct start_play;
	AudioServerAct advance;

	SoundToken skip_token;
	SoundToken loop_token;

	r_bool index_ready;
	AuIndexRec index[sound_num_tokens];
	//AuIndexRec index[2];
} AudioServer;

void init_audio_server(AudioServer *as, Network *network, uint8_t timer_id);
void _aserv_skip_to_clip(AudioServer *aserv, SoundToken cur_token, SoundToken loop_token);

// visibility for debugging
void _aserv_fetch_start(AudioServerAct *asa);

#endif // _audio_server_h
