#include "remote_bbuf.h"

void rbs_update(RemoteBBufSend *rbs);
void rbs_send_complete(SendSlot *slot);
void rbr_recv(RecvSlot *recvSlot);

#define REMOTE_BBUF_SEND_RATE 33000	// 30 board msgs ber sec

void init_remote_bbuf_send(RemoteBBufSend *rbs, Network *network)
{
	rbs->func = (ActivationFunc) rbs_update;
	rbs->network = network;
	rbs->sendSlot.func = rbs_send_complete;
	rbs->sendSlot.msg = (Message*) rbs->send_msg_alloc;
	rbs->sendSlot.sending = FALSE;
	memset(rbs->offscreen, 0, REMOTE_BBUF_NUM_BOARDS*NUM_DIGITS*sizeof(SSBitmap));
	memset(rbs->changed, FALSE, REMOTE_BBUF_NUM_BOARDS*sizeof(r_bool));
	rbs->last_index = 0;

	schedule_us(1, (Activation*) rbs);
}

void rbs_send_complete(SendSlot *slot)
{
	slot->sending = FALSE;
}

void send_remote_bbuf(RemoteBBufSend *rbs, SSBitmap *bm, uint8_t index, uint8_t mask)
{
	assert(0<=index && index<REMOTE_BBUF_NUM_BOARDS);
	
	int di;
	for (di=0; di<NUM_DIGITS; di++)
	{
		if (mask & 0x80)
		{
			rbs->offscreen[index][di] = bm[di];
		}
		mask<<=1;
	}
	rbs->changed[index] = TRUE;
}

int rbs_find_changed_index(RemoteBBufSend *rbs)
{
	int idx = rbs->last_index;
	int tries;
	for (tries=0; tries<REMOTE_BBUF_NUM_BOARDS; tries++)
	{
		idx = (idx + 1) % REMOTE_BBUF_NUM_BOARDS;
		if (rbs->changed[idx])
		{
			return idx;
		}
	}
	return -1;
}

void rbs_update(RemoteBBufSend *rbs)
{
	schedule_us(REMOTE_BBUF_SEND_RATE, (Activation*) rbs);

	if (rbs->sendSlot.sending)
	{
		LOGF((logfp, "rbs_update: busy\n"));
		return;
	}

	int index = rbs_find_changed_index(rbs);
	if (index==-1)
	{
		// no changed lines
		LOGF((logfp, "rbs_update: idle\n"));
		return;
	}

	LOGF((logfp, "rbs_update: update[%d]\n", index));

	// send a packet for this changed line
	rbs->sendSlot.msg->dest_port = REMOTE_BBUF_PORT;
	rbs->sendSlot.msg->message_size = sizeof(BBufMessage);
	BBufMessage *bbm = (BBufMessage *) &rbs->sendSlot.msg->data;
	memcpy(bbm->buf, rbs->offscreen[index], NUM_DIGITS);
	bbm->index = index;
	net_send_message(rbs->network, &rbs->sendSlot);

	rbs->changed[index] = FALSE;
	rbs->last_index = index;
}

void init_remote_bbuf_recv(RemoteBBufRecv *rbr, Network *network)
{
	rbr->recvSlot.func = rbr_recv;
	rbr->recvSlot.port = REMOTE_BBUF_PORT;
	rbr->recvSlot.payload_capacity = sizeof(BBufMessage);
	rbr->recvSlot.msg_occupied = FALSE;
	rbr->recvSlot.msg = (Message*) rbr->recv_msg_alloc;

	net_bind_receiver(network, &rbr->recvSlot);
}

void rbr_recv(RecvSlot *recvSlot)
{
	BBufMessage *bbm = (BBufMessage *) &recvSlot->msg->data;
	board_buffer_paint(bbm->buf, bbm->index, 0xff);
	recvSlot->msg_occupied = FALSE;
}
