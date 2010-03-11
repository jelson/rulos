#include "rocket.h"

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


static uint8_t net_compute_checksum(char *buf, int size)
{
	int i;
	uint8_t cksum = 0x67;
	for (i=0; i<size; i++)
	{
		cksum = cksum*131 + buf[i];
	}
	return cksum;
}


//////////// Network Init ////////////////////////////////////

static void net_recv_upcall(TWIRecvSlot *trs, uint8_t len);

void init_network(Network *net, Addr local_addr)
{
	int i;
	for (i=0; i<MAX_LISTENERS; i++)
	{
		net->recvSlots[i] = NULL;
	}

	SendSlotPtrQueue_init(SendQueue(net), sizeof(net->sendQueue_storage));

	// Initialize the receive slot for the underlying physical
	// network.
	TWIRecvSlot *trs = (TWIRecvSlot *) net->TWIRecvSlotStorage;
	trs->func = net_recv_upcall;
	trs->capacity = sizeof(Message) + NET_MAX_PAYLOAD_SIZE;
	trs->occupied = FALSE;
	trs->user_data = net;

	// Set up underlying physical network
	hal_twi_init(local_addr, trs);
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


// Called by the TWI code when a packet arrives in our receive slot.
static void net_recv_upcall(TWIRecvSlot *trs, uint8_t len)
{
	Network *net = (Network *) trs->user_data;
	Message *msg = (Message *) trs->data;

	// make sure it's at least as long as we're expecting
	if (len < sizeof(Message))
	{
		LOGF((logfp, "netstack error: got short pkt only %d bytes\n", len));
		goto done;
	}

	// make sure the checksum matches
	uint8_t incoming_checksum = msg->checksum;
	msg->checksum = 0;

	if (net_compute_checksum(trs->data, len) != incoming_checksum)
	{
		LOGF((logfp, "netstack error: checksum mismatch\n"));
		goto done;
	}

	// find the receive slot, if any 
	uint8_t slotIdx = net_find_receive_slot(net, msg->dest_port);

	if (slotIdx == SLOT_NONE)
	{
		LOGF((logfp, "netstack error: dropped packet to port %d (0x%x) with no listener\n",
			  msg->dest_port, msg->dest_port));
		goto done;
	}

	RecvSlot *rs = net->recvSlots[slotIdx];

	// if slot is occupied, drop
	if (rs->msg_occupied)
	{
		LOGF((logfp, "netstack error: dropped packet to port %d (0x%x) with full buffer\n",
			  msg->dest_port, msg->dest_port));
		goto done;
	}

	uint8_t payload_len = len - sizeof(Message);

	// make sure slot has enough space
	if (rs->payload_capacity < payload_len)
	{
		LOGF((logfp, "netstack error: port %d got payload of len %d, only had capacity for %d\n",
			  msg->dest_port, payload_len, rs->payload_capacity));
		goto done;
	}
	
	// everything seems good!  copy to the receive slot and call the callback
	memcpy(rs->msg, msg, len);
	trs->occupied = FALSE; // do this now in case the callback is slow
	rs->func(rs, payload_len);
	return;
	
done:
	trs->occupied = FALSE;
}

/////////////// Sending ///////////////////////////////////////////////

static void net_send_next_message_down(Network *net);
static void net_twi_send_done(void *user_data);

// External visible API from above to launch a packet.
r_bool net_send_message(Network *net, SendSlot *sendSlot)
{
	LOGF((logfp, "netstack: sending %d-byte payload to %d:%d (0x%x:0x%x)\n",
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

	// Make sure this guy thinks he's sending.
	assert(sendSlot->sending == TRUE);

	// compute length and checksum.
	uint8_t len = sizeof(Message) + sendSlot->msg->payload_len;
	sendSlot->msg->checksum = 0;
	sendSlot->msg->checksum = net_compute_checksum((char *) sendSlot->msg, len);

	// send down
	hal_twi_send(sendSlot->dest_addr, (char *) sendSlot->msg, len,
				 net_twi_send_done, net);
}


// Called when TWI has finished sending down the stack.
// Mark the SendSlot's buffer as free and call the callback.
static void net_twi_send_done(void *user_data)
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

