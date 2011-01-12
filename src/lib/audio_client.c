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

#include "audio_client.h"

extern void syncdebug(uint8_t spaces, char f, uint16_t line);
#define SYNCDEBUG()	syncdebug(0, 'U', __LINE__)
//#define SYNCDEBUG()	{}

void ac_send_complete(SendSlot *sendSlot);

void init_audio_client(AudioClient *ac, Network *network)
{
	ac->network = network;

	ac->sendSlot.func = NULL;
	ac->sendSlot.msg = (Message*) ac->send_msg_alloc;
	ac->sendSlot.sending = FALSE;
}


r_bool ac_skip_to_clip(AudioClient *ac, uint8_t stream_idx, SoundToken cur_token, SoundToken loop_token)
{
	if (ac->sendSlot.sending)
	{
		return FALSE;
	}

	ac->sendSlot.dest_addr = AUDIO_ADDR;
	ac->sendSlot.msg->dest_port = AUDIO_PORT;
	ac->sendSlot.msg->payload_len = sizeof(AudioRequestMessage);
	AudioRequestMessage *arm = (AudioRequestMessage *) &ac->sendSlot.msg->data;
	arm->stream_idx = stream_idx;
	arm->skip = TRUE;
	arm->skip_cmd.token = cur_token;
	arm->skip_cmd.mlvolume = 0;
	arm->loop_cmd.token = loop_token;
	arm->loop_cmd.mlvolume = 0;
	net_send_message(ac->network, &ac->sendSlot);

	return TRUE;
}

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
	arm->stream_idx = stream_idx;
	arm->skip = FALSE;
	arm->skip_cmd.token = -1;
	arm->loop_cmd.token = loop_token;
	arm->loop_cmd.mlvolume = 0;
	net_send_message(ac->network, &ac->sendSlot);

	return TRUE;
}


