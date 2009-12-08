#include "audio_server.h"

void ac_send_complete(SendSlot *sendSlot);

void init_audio_client(AudioClient *ac, Network *network)
{
	ac->network = network;

	ac->sendSlot.func = ac_send_complete;
	ac->sendSlot.msg = (Message*) ac->send_msg_alloc;
	ac->sendSlot.sending = FALSE;
}

void ac_send_complete(SendSlot *sendSlot)
{
	sendSlot->sending = FALSE;
}

r_bool ac_skip_to_clip(AudioClient *ac, SoundToken cur_token, SoundToken loop_token)
{
	if (ac->sendSlot.sending)
	{
		return FALSE;
	}
	ac->sendSlot.msg->dest_port = AUDIO_PORT;
	ac->sendSlot.msg->message_size = sizeof(AudioRequestMessage);
	AudioRequestMessage *arm = (AudioRequestMessage *) &ac->sendSlot.msg->data;
	arm->skip = TRUE;
	arm->skip_token = cur_token;
	arm->loop_token = loop_token;
	net_send_message(ac->network, &ac->sendSlot);
	return TRUE;
}

r_bool ac_queue_loop_clip(AudioClient *ac, SoundToken loop_token)
{
	if (ac->sendSlot.sending)
	{
		return FALSE;
	}
	ac->sendSlot.msg->dest_port = AUDIO_PORT;
	ac->sendSlot.msg->message_size = sizeof(AudioRequestMessage);
	AudioRequestMessage *arm = (AudioRequestMessage *) &ac->sendSlot.msg->data;
	arm->skip = FALSE;
	arm->skip_token = -1;
	arm->loop_token = loop_token;
	net_send_message(ac->network, &ac->sendSlot);
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////

void as_update(AudioServer *as);
void as_recv(RecvSlot *recvSlot);
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
	as->recv_this = as;

	board_buffer_init(&as->bbuf);
	board_buffer_push(&as->bbuf, board0);

	net_bind_receiver(network, &as->recvSlot);

	schedule_us(1, (Activation*) as);
}

void as_recv(RecvSlot *recvSlot)
{
	AudioServer *as = *(AudioServer**)(recvSlot+1);
	AudioRequestMessage *arm = (AudioRequestMessage *) &recvSlot->msg->data;
	if (arm->skip)
	{
// TODO left off here
//		ad_skip_to_clip(as->ad, arm->skip_token, arm->loop_token);
	}
	else
	{
//		ad_queue_loop_clip(as->ad, arm->loop_token);
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
	int_to_string2(buf, 4, 0, as->ad->cur_token);
	int_to_string2(buf+4, 4, 0, as->ad->loop_token);
	ascii_to_bitmap_str(as->bbuf.buffer, 8, buf);
	board_buffer_draw(&as->bbuf);
}
