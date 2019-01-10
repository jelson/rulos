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

#include <string.h>

#include "core/clock.h"
#include "core/rulos.h"
#include "periph/7seg_panel/remote_bbuf.h"

#define REMOTE_BBUF_SEND_RATE 10000	// 100 board msgs per sec

void init_remote_bbuf_send(RemoteBBufSend *rbs, Network *network)
{
	rbs->network = network;
	rbs->sendSlot.func = NULL;
	rbs->sendSlot.msg = (Message*) rbs->send_msg_alloc;
	rbs->sendSlot.sending = FALSE;
#if NUM_REMOTE_BOARDS > 0
	memset(rbs->offscreen, 0, NUM_REMOTE_BOARDS*NUM_DIGITS*sizeof(SSBitmap));
#endif
	memset(rbs->changed, FALSE, NUM_REMOTE_BOARDS*sizeof(r_bool));
	rbs->last_index = 0;

	schedule_us(1, (ActivationFuncPtr) rbs_update, rbs);
}

void send_remote_bbuf(RemoteBBufSend *rbs, SSBitmap *bm, uint8_t index, uint8_t mask)
{
	assert(0<=index && index<NUM_REMOTE_BOARDS);
	
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
#if NUM_REMOTE_BOARDS > 0	
	int idx = rbs->last_index;
	int tries;
	for (tries=0; tries<NUM_REMOTE_BOARDS; tries++)
	{
		idx = (idx + 1) % NUM_REMOTE_BOARDS;
		if (rbs->changed[idx])
		{
			return idx;
		}
	}
#endif
	return -1;
}

void rbs_update(RemoteBBufSend *rbs)
{
	schedule_us(REMOTE_BBUF_SEND_RATE, (ActivationFuncPtr) rbs_update, rbs);

	if (rbs->sendSlot.sending)
	{
		LOG("rbs_update: busy\n");
		return;
	}

	int index = rbs_find_changed_index(rbs);
	if (index==-1)
	{
		// no changed lines
		LOG("rbs_update: idle\n");
		return;
	}

	LOG("rbs_update: update[%d]\n", index);

	// send a packet for this changed line
	rbs->sendSlot.dest_addr = DONGLE_BASE_ADDR + index;
	rbs->sendSlot.msg->dest_port = REMOTE_BBUF_PORT;
	rbs->sendSlot.msg->payload_len = sizeof(BBufMessage);
	BBufMessage *bbm = (BBufMessage *) &rbs->sendSlot.msg->data;
	memcpy(bbm->buf, rbs->offscreen[index], NUM_DIGITS);
    bbm->index = index;

	if (net_send_message(rbs->network, &rbs->sendSlot))
	{
		rbs->changed[index] = FALSE;
	}

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

void rbr_recv(RecvSlot *recvSlot, uint8_t payload_len)
{
	if (payload_len != sizeof(BBufMessage)) {
		LOG("Error: expected BBufMessage of size %ld, got %d bytes",
		    sizeof(BBufMessage), payload_len);
		return;
	}

	BBufMessage *bbm = (BBufMessage *) &recvSlot->msg->data;
	board_buffer_paint(bbm->buf, bbm->index, 0xff);
	recvSlot->msg_occupied = FALSE;
}
