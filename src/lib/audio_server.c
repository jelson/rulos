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

#include "audio_server.h"

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
	arm->skip_token = cur_token;
	arm->loop_token = loop_token;
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
	arm->skip_token = -1;
	arm->loop_token = loop_token;
	net_send_message(ac->network, &ac->sendSlot);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////

void as_update(AudioServer *as);
void as_recv(RecvSlot *recvSlot, uint8_t payload_len);
void as_update_display(AudioServer *as);

void init_audio_server(AudioServer *as, AudioDriver *ad, Network *network, uint8_t board0)
{
	as->func = (ActivationFunc) as_update;
	as->ad = ad;
	as->recvSlot.func = as_recv;
	as->recvSlot.port = AUDIO_PORT;
	as->recvSlot.payload_capacity = sizeof(AudioRequestMessage);
	as->recvSlot.msg_occupied = FALSE;
	as->recvSlot.msg = (Message*) as->recv_msg_alloc;
	as->recvSlot.user_data = as;

	board_buffer_init(&as->bbuf DBG_BBUF_LABEL("audio_server"));
	board_buffer_push(&as->bbuf, board0);

	net_bind_receiver(network, &as->recvSlot);

	schedule_us(1, (Activation*) as);
}

void as_recv(RecvSlot *recvSlot, uint8_t payload_len)
{
	AudioServer *as = (AudioServer *) recvSlot->user_data;
	AudioRequestMessage *arm = (AudioRequestMessage *) &recvSlot->msg->data;

	assert(payload_len == sizeof(AudioRequestMessage));

	if (arm->skip)
	{
		ad_skip_to_clip(as->ad, arm->stream_idx, arm->skip_token, arm->loop_token);
	}
	else
	{
		ad_queue_loop_clip(as->ad, arm->stream_idx, arm->loop_token);
	}

	as_update_display(as);
}

void as_update(AudioServer *as)
{
	schedule_us(25000, (Activation*) as);
	as_update_display(as);
}

void as_update_display(AudioServer *as)
{
	char buf[9];
	AudioStream *astream = &as->ad->stream[0];
	int_to_string2(buf, 4, 0, astream->cur_token);
	int_to_string2(buf+4, 4, 0, astream->loop_token);
	ascii_to_bitmap_str(as->bbuf.buffer, 8, buf);
	board_buffer_draw(&as->bbuf);
}
