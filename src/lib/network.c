#include "network.h"
#include "util.h"

#include "queue.mc"
QUEUE_DEFINE(SendSlotPtr)

#define SendQueue(n) ((SendSlotPtrQueue*) n->sendQueue_storage)

void net_update(Network *net);
void net_timer_update(NetworkTimer *network_timer);
uint8_t net_compute_checksum(uint8_t *buf, int size);
void net_extract_received_message(Network *net);
void net_log_buffer(uint8_t *buf, int len);
void net_recv_discard(Network *net, uint8_t count, r_bool log);
void net_heartbeat(Network *net);
r_bool net_timeout(Network *net);
r_bool net_send_buffer_empty(Network *net);

void init_network(Network *net)
{
	net->func = (ActivationFunc) net_update;

	int i;
	for (i=0; i<MAX_LISTENERS; i++)
	{
		net->recvSlots[i] = NULL;
	}

	SendSlotPtrQueue_init(SendQueue(net), sizeof(net->sendQueue_storage));

	net->send_index = 0;
	net->send_size = 0;
	net->recv_index = 0;

	net_heartbeat(net);

	// timer runs on a separate "thread", so we don't end up launching
	// an extra unending cascade of timeouts for every byte we process.
	net->network_timer.func = (ActivationFunc) net_timer_update;
	net->network_timer.net = net;

	hal_twi_init((Activation *) net);
	schedule_us(1, (Activation*) &net->network_timer);
}

int net_find_slot(Network *net, Port port)
{
	int i;
	for (i=0; i<MAX_LISTENERS; i++)
	{
		if (net->recvSlots[i]==NULL)
		{
			continue;
		}
		if (net->recvSlots[i]->msg_occupied)
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
int net_find_empty_slot(Network *net)
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

void net_bind_receiver(Network *net, RecvSlot *recvSlot)
{
	int slotIdx = net_find_empty_slot(net);
	assert(slotIdx != SLOT_NONE);
	net->recvSlots[slotIdx] = recvSlot;
}

r_bool net_send_message(Network *net, SendSlot *sendSlot)
{
	r_bool need_wake = (SendSlotPtrQueue_length(SendQueue(net))>0);

	r_bool fit = SendSlotPtrQueue_append(SendQueue(net), sendSlot);
	if (fit)
	{
		sendSlot->sending = TRUE;
	}
	if (need_wake)
	{
		schedule_now((Activation*) net);
	}
	return fit;
}

// packet format:
// NET_SYNC
// NET_PORT
// NET_PAYLOAD_SIZE:n
// [n*uint8_t]
// <checksum>

r_bool net_send_buffer_empty(Network *net)
{
	return net->send_index >= net->send_size;
}

void net_pop_message_to_send(Network *net)
{
	assert(net_send_buffer_empty(net));

	SendSlot *ss;
	r_bool rc = SendSlotPtrQueue_pop(SendQueue(net), &ss);
	assert(rc);

	// 2: NET_SYNC & checksum fields
	assert(ss->msg->message_size <= NET_MAX_MESSAGE_SIZE - sizeof(Message) - 2);

	uint8_t *buf = net->send_buffer;
	int off = 0;
	buf[off] = NET_SYNC;
	off+=1;
	memmove(&buf[off], ss->msg, sizeof(Message));
	off+=sizeof(Message);
	memmove(&buf[off], ss->msg->data, ss->msg->message_size);
	off+=ss->msg->message_size;
	buf[off] = net_compute_checksum(net->send_buffer, off);
	off+=1;
	net->send_size = off;
	net->send_index = 0;

	//LOGF((logfp, "popped to send: "));
	//net_log_buffer(net->send_buffer, net->send_size);

	ss->func(ss);
}

void net_update(Network *net)
{
	//LOGF((logfp, "net_update(%.2f)\n", (double)(clock_time_us())/1000000.0 ));

	// Send a byte
	if (hal_twi_ready_to_send()
		&& !net_send_buffer_empty(net))
	{
		//LOGF((logfp, "send byte\n"));
		hal_twi_send_byte(net->send_buffer[net->send_index]);
		net->send_index += 1;
	}

	// Shift in a new message to send, if nothing on send buffer
	if (net_send_buffer_empty(net)
		&& SendSlotPtrQueue_length(SendQueue(net))>0)
	{
		net_pop_message_to_send(net);
	}

	// Read a byte
	uint8_t byte;
	if (hal_twi_read_byte(&byte))
	{
		assert(net->recv_index < NET_MAX_MESSAGE_SIZE - 1);
		net->recv_buffer[net->recv_index] = byte;
		net->recv_index += 1;
		net_extract_received_message(net);
	}
}

void net_extract_received_message(Network *net)
{
	while (TRUE)
	{
		if (net->recv_index == 0)
		{
			// buffer empty; wait
			break;
		}

		if (net->recv_buffer[0]!=NET_SYNC)
		{
			// Uh oh, message out of sync
			LOGF((logfp, "resync: "));
			net_recv_discard(net, 1, TRUE);
			continue;
		}

		if (net->recv_index < 3)
		{
			// payload size not available yet; wait.
			break;
		}

		uint8_t payload_size = net->recv_buffer[2];
		uint8_t packet_size = payload_size + 4;
		
		if (packet_size > NET_MAX_MESSAGE_SIZE)
		{
			LOGF((logfp, "packet too big (%02x > %02x); resyncing\n",
				packet_size, NET_MAX_MESSAGE_SIZE));
			// discard every byte we've got left -- can't run into another
			// legitimate packet, since this one claimed to be longer than
			// our buffer.
			net_recv_discard(net, net->recv_index, TRUE);
			// this will immediately fall out with empty buffer
			continue;
		}

		if (net->recv_index < packet_size)
		{
			// entire message not yet available.
			break;
		}

		uint8_t received_checksum = net->recv_buffer[packet_size-1];
		uint8_t computed_checksum = net_compute_checksum(net->recv_buffer, packet_size-1);
		if (received_checksum != computed_checksum)
		{
			LOGF((logfp, "discarding %d-byte packet; received checksum %02x, computed %02x\n",
				packet_size, received_checksum, computed_checksum));
			net_recv_discard(net, packet_size, TRUE);
			continue;
		}

		// Okay, we've got a fully-formed, checksum-matching packet
		Port port = net->recv_buffer[1];
		uint8_t recv_slot_idx = net_find_slot(net, port);
		if (recv_slot_idx == SLOT_NONE)
		{
			LOGF((logfp, "discarding packet for unbound port %02x\n", port));
			net_recv_discard(net, packet_size, TRUE);
			continue;
		}

		RecvSlot *rs = net->recvSlots[recv_slot_idx];
		if (rs->msg_occupied)
		{
			LOGF((logfp, "discarding packet due to busy port %02x\n", port));
			net_recv_discard(net, packet_size, TRUE);
			continue;
		}

		if (payload_size > rs->payload_capacity)
		{
			LOGF((logfp, "discarding oversized packet for port %02x\n", port));
			net_recv_discard(net, packet_size, TRUE);
			continue;
		}

		{
			// Yay! We can deliver the packet!
			memmove(rs->msg, &net->recv_buffer[1], sizeof(Message)+payload_size);
//			LOGF((logfp, "Received valid packet: "));
			net_recv_discard(net, packet_size, FALSE);

			rs->func(rs);
			continue;
		}
	}

	// there should always be room to write more bytes.
	assert(net->recv_index < NET_MAX_MESSAGE_SIZE -1);
}

void net_log_buffer(uint8_t *buf, int len)
{
	int i;
	for (i=0; i<len; i++)
	{
		LOGF((logfp, "%02x ", buf[i]));
	}
	LOGF((logfp, "\n"));
}

// TODO inefficient shift
void net_recv_discard(Network *net, uint8_t count, r_bool log)
{
	if (log)
	{
		LOGF((logfp, "discarding: "));
		net_log_buffer(net->recv_buffer, count);
	}

	assert(net->recv_index >= count);
	memmove(net->recv_buffer, net->recv_buffer+count, net->recv_index-count);
	net->recv_index -= count;
}

void net_timer_update(NetworkTimer *network_timer)
{
	//LOGF((logfp, "running net_update from network_timer\n"));
	net_update(network_timer->net);
	schedule_us(NET_TIMEOUT, (Activation*) network_timer);
}

uint8_t net_compute_checksum(uint8_t *buf, int size)
{
	int i;
	uint8_t cksum = 0x67;
	for (i=0; i<size; i++)
	{
		cksum = cksum*131 + buf[i];
	}
	return cksum;
}

void net_heartbeat(Network *net)
{
	net->last_activity = clock_time_us();
}

r_bool net_timeout(Network *net)
{
	return later_than(clock_time_us(), net->last_activity + NET_TIMEOUT);
}
