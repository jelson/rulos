#ifndef _audio_server_h
#define _audio_server_h

#include "rocket.h"
#include "network.h"
#include "network_ports.h"
#include "audio_driver.h"

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


typedef struct s_audio_server {
	ActivationFunc func;

	AudioDriver *ad;
	uint8_t recv_msg_alloc[sizeof(Message)+sizeof(AudioRequestMessage)];

	RecvSlot recvSlot;

	BoardBuffer bbuf;
} AudioServer;

void init_audio_server(AudioServer *as, AudioDriver *ad, Network *network, uint8_t board0);

#endif // _audio_server_h
