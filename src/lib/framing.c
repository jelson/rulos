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

#define NET_SYNC 	(0xf3)

void net_extract_received_message(Network *net);
void net_recv_discard(Network *net, uint8_t count, r_bool log);


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

