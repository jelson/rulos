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

#include "rocket.h"
#include "net_compute_checksum.h"

#include "queue.mc"
QUEUE_DEFINE(SendSlotPtr)

#define SendQueue(n) ((SendSlotPtrQueue*) n->sendQueue_storage)

///////////////// Utilities ////////////////////////////////////

#if 0
void net_log_buffer(uint8_t *buf, int len)
{
	int i;
	for (i=0; i<len; i++)
	{
		LOGF((logfp, "%02x ", buf[i]));
	}
	LOGF((logfp, "\n"));
}
#endif

//////////// Network Init ////////////////////////////////////

static void net_recv_upcall(MediaRecvSlot *mrs, uint8_t len);

void init_network(Network *net, MediaStateIfc *media)
{
	int i;
	for (i=0; i<MAX_LISTENERS; i++)
	{
		net->recvSlots[i] = NULL;
	}

	SendSlotPtrQueue_init(SendQueue(net), sizeof(net->sendQueue_storage));

	MediaRecvSlot *mrs = &net->mrs;
	mrs->func = net_recv_upcall;
	mrs->capacity = sizeof(net->MediaRecvSlotStorage) - sizeof(MediaRecvSlot);
	mrs->occupied_len = 0;
	mrs->user_data = net;

	// Set up underlying physical network
	net->media = media;
}


/////////////// Receiving /////////////////////////////////

static int net_find_receive_slot(Network *net, Port port)
{
	int i;
	for (i=0; i<MAX_LISTENERS; i++)
	{
		if (net->recvSlots[i]==NULL)
		{
			continue;
		}
		if (net->recvSlots[i]->port==port)
		{
			return i;
		}
	}
	return SLOT_NONE;
}

static int net_find_empty_receive_slot(Network *net)
{
	int i;
	for (i=0; i<MAX_LISTENERS; i++)
	{
		if (net->recvSlots[i]==NULL)
		{
			return i;
		}
	}
	return SLOT_NONE;
}

// Public API function to start listening on a port
void net_bind_receiver(Network *net, RecvSlot *recvSlot)
{
	uint8_t slotIdx = net_find_empty_receive_slot(net);
	assert(slotIdx != SLOT_NONE);
	net->recvSlots[slotIdx] = recvSlot;
	LOGF((logfp, "netstack: listener bound to port %d (0x%x), slot %d\n",
		  recvSlot->port, recvSlot->port, slotIdx));
}


// Called by the media layer when a packet arrives in our receive slot.
static void net_recv_upcall(MediaRecvSlot *mrs, uint8_t len)
{
	Network *net = (Network *) mrs->user_data;
	Message *msg = (Message *) mrs->data;
	uint8_t incoming_checksum;
	uint8_t slotIdx;
	uint8_t payload_len;
	uint8_t oldInterrupts;
	RecvSlot *rs;
	
	// make sure it's at least as long as we're expecting
	if (len < sizeof(Message))
	{
		LOGF((logfp, "netstack error: got short pkt only %d bytes\n", len));
#ifdef DEBUG_STACK_WITH_UART
		hal_uart_sync_send("drp1", 4);
#endif
		goto done;
	}

	// make sure the checksum matches
	incoming_checksum = msg->checksum;
	msg->checksum = 0;

	if (net_compute_checksum((char *) msg, len) != incoming_checksum)
	{
		LOGF((logfp, "netstack error: checksum mismatch\n"));
#ifdef DEBUG_STACK_WITH_UART
		hal_uart_sync_send("drp2", 4);
		hal_uart_sync_send((char *)msg, len);
#endif
		goto done;
	}

	// find the receive slot, if any 
	slotIdx = net_find_receive_slot(net, msg->dest_port);

	if (slotIdx == SLOT_NONE)
	{
		LOGF((logfp, "netstack error: dropped packet to port %d (0x%x) with no listener\n",
			  msg->dest_port, msg->dest_port));
#ifdef DEBUG_STACK_WITH_UART
		hal_uart_sync_send("drp3", 4);
#endif
		goto done;
	}

	rs = net->recvSlots[slotIdx];

	// if slot is occupied, drop
	if (rs->msg_occupied)
	{
		LOGF((logfp, "netstack error: dropped packet to port %d (0x%x) with full buffer\n",
			  msg->dest_port, msg->dest_port));
#ifdef DEBUG_STACK_WITH_UART
		hal_uart_sync_send("drp4", 4);
#endif
		goto done;
	}

	payload_len = len - sizeof(Message);

	// make sure slot has enough space
	if (rs->payload_capacity < payload_len)
	{
		LOGF((logfp, "netstack error: port %d got payload of len %d, only had capacity for %d\n",
			  msg->dest_port, payload_len, rs->payload_capacity));
#ifdef DEBUG_STACK_WITH_UART
		hal_uart_sync_send("drp5", 4);
#endif
		goto done;
	}
	
	// everything seems good!  copy to the receive slot and call the callback
	memcpy(rs->msg, msg, len);

	// tell the network stack the buffer is free
	oldInterrupts = hal_start_atomic();
	mrs->occupied_len = 0;
	hal_end_atomic(oldInterrupts);
	
#ifdef DEBUG_STACK_WITH_UART
	hal_uart_sync_send("CP", 2);
#endif
	(rs->func)(rs, payload_len);
#ifdef DEBUG_STACK_WITH_UART
	hal_uart_sync_send("p", 1);
#endif
	return;
	
done:
	oldInterrupts = hal_start_atomic();
	mrs->occupied_len = 0;
	hal_end_atomic(oldInterrupts);
}

/////////////// Sending ///////////////////////////////////////////////

static void net_send_next_message_down(Network *net);
static void net_send_done_cb(void *user_data);

// External visible API from above to launch a packet.
r_bool net_send_message(Network *net, SendSlot *sendSlot)
{
	LOGF((logfp, "netstack: queueing %d-byte payload to %d:%d (0x%x:0x%x)\n",
		  sendSlot->msg->payload_len,
		  sendSlot->dest_addr, sendSlot->msg->dest_port,
		  sendSlot->dest_addr, sendSlot->msg->dest_port))

	assert(sendSlot->sending == FALSE);
	r_bool need_wake = (SendSlotPtrQueue_length(SendQueue(net)) == 0);
	r_bool fit = SendSlotPtrQueue_append(SendQueue(net), sendSlot);

	if (fit)
	{
		sendSlot->sending = TRUE;
	}
	if (need_wake)
	{
		net_send_next_message_down(net);
	}
	return fit;
}

// Send a message down the stack.
static void net_send_next_message_down(Network *net)
{
	// Get the next sendSlot out of our queue.  Return if none.
	SendSlot *sendSlot;
	r_bool rc = SendSlotPtrQueue_peek(SendQueue(net), &sendSlot);

	if (rc == FALSE)
		return;

	LOGF((logfp, "netstack: releasing %d-byte payload to %d:%d (0x%x:0x%x)\n",
		  sendSlot->msg->payload_len,
		  sendSlot->dest_addr, sendSlot->msg->dest_port,
		  sendSlot->dest_addr, sendSlot->msg->dest_port))

	// Make sure this guy thinks he's sending.
	assert(sendSlot->sending == TRUE);

	// compute length and checksum.
	uint8_t len = sizeof(Message) + sendSlot->msg->payload_len;
	assert(len>0);
	sendSlot->msg->checksum = 0;
	sendSlot->msg->checksum = net_compute_checksum((char *) sendSlot->msg, len);

	// send down
	(net->media->send)(net->media,
		sendSlot->dest_addr, (char *) sendSlot->msg, len,
		net_send_done_cb, net);
}


// Called when media has finished sending down the stack.
// Mark the SendSlot's buffer as free and call the callback.
static void net_send_done_cb(void *user_data)
{
	Network *net = (Network *) user_data;
	SendSlot *sendSlot;
	r_bool rc = SendSlotPtrQueue_pop(SendQueue(net), &sendSlot);

	assert(rc);
	assert(sendSlot->sending == TRUE);

	// mark the packet as no-longer-sending and call the user's
	// callback
	sendSlot->sending = FALSE;
	if (sendSlot->func)
		sendSlot->func(sendSlot);

	// launch the next queued packet if any
	net_send_next_message_down(net);
}

//////////////////////////////////////////////////////////////////////////////

void init_twi_network(Network *net, uint32_t speed_khz, Addr local_addr)
{
	init_network(net, hal_twi_init(speed_khz, local_addr, &net->mrs));
}
