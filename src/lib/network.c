#include "network.h"
#include "util.h"

#include "queue.mc"
QUEUE_DEFINE(SendSlotPtr)

#define SendQueue(n) ((SendSlotPtrQueue*) n->sendQueue_storage)

void net_update(Network *net);
void net_timer_update(NetworkTimer *network_timer);
void net_got_payload(Network *net);
void net_sent_payload(Network *net);
void net_init_checksum(uint8_t *cksum);
void net_update_checksum(uint8_t *cksum, uint8_t byte);
void net_heartbeat(Network *net);
r_bool net_timeout(Network *net);

void init_network(Network *net)
{
	net->func = (ActivationFunc) net_update;

	int i;
	for (i=0; i<MAX_LISTENERS; i++)
	{
		net->recvSlots[i] = NULL;
	}

	SendSlotPtrQueue_init(SendQueue(net), SEND_QUEUE_SIZE);

	net->state = NET_STATE_IDLE;

	net_heartbeat(net);

	// timer runs on a separate "thread", so we don't end up launching
	// an extra unending cascade of timeouts for every byte we process.
	net->network_timer.func = (ActivationFunc) net_timer_update;
	net->network_timer.net = net;

	hal_twi_init((Activation *) net);
	schedule_us(1, (Activation*) &net->network_timer);
}

int net_find_slot(Network *net, uint8_t port)
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
	r_bool fit = SendSlotPtrQueue_append(SendQueue(net), sendSlot);
	if (fit)
	{
		sendSlot->sending = TRUE;
	}
	return fit;
}

// packet format:
// NET_SYNC
// NET_PORT
// NET_PAYLOAD_SIZE:n
// [n*uint8_t]
// <checksum>

void net_update(Network *net)
{
	//LOGF((logfp, "net_update(%.2f)\n", (double)(clock_time_us())/1000000.0 ));
	uint8_t byte;
	if (hal_twi_read_byte(&byte))
	{
		//LOGF((logfp, "Net collects byte %02x\n", byte));
		switch (net->state)
		{
			case NET_STATE_IDLE:
			case NET_STATE_RESYNC:
				if (byte==NET_SYNC) {
					net->state = NET_STATE_RECV_PORT;
				} else {
					net->state = NET_STATE_RESYNC;
				}
				break;
			case NET_STATE_RECV_PORT:
				net->slot = net_find_slot(net, byte);
				if (net->slot != SLOT_NONE)
				{
					RecvSlot *rs = net->recvSlots[net->slot];
					rs->msg->dest_port = byte;
					assert(rs->msg->dest_port == rs->port);
				}
				net->state = NET_STATE_RECV_PAYLOAD_SIZE;
				break;
			case NET_STATE_RECV_PAYLOAD_SIZE:
				net->payload_remaining = byte;
				if (net->slot != SLOT_NONE)
				{
					RecvSlot *rs = net->recvSlots[net->slot];
					if (rs->payload_capacity < net->payload_remaining)
					{
						// can't deliver. Drop.
						net->slot = SLOT_NONE;
					}
					else
					{
						// ready to start appending bytes
						rs->msg->message_size = 0;
					}
				}
				net_init_checksum(&net->checksum);
				net_got_payload(net);
				break;
			case NET_STATE_RECV_PAYLOAD:
				if (net->slot != SLOT_NONE)
				{
					RecvSlot *rs = net->recvSlots[net->slot];
					rs->msg->data[rs->msg->message_size++] = byte;
				}
				net->payload_remaining -= 1;
				net_update_checksum(&net->checksum, byte);
				net_got_payload(net);
				break;
			case NET_STATE_RECV_CHECKSUM:
				if (net->checksum != byte)
				{
					// bogus packet. Drop.
					LOGF((logfp, "computed checksum %02x, received checksum %02x.", net->checksum, byte));
					// could be because we were mid-packet, and this
					// wasn't really a checksum.
					// Better wait for a timeout.
					net->state = NET_STATE_RESYNC;
				}
				else
				{
					RecvSlot *rs = net->recvSlots[net->slot];
					rs->msg_occupied = TRUE;
					rs->func(rs);
					net->state = NET_STATE_IDLE;

					// wake up again immediately to see if we want to send
					// TODO a random delay might be wise here, to see
					// if someone else is planning to send.
					schedule_us(1, (Activation*) net);
				}
				break;
			case NET_STATE_SEND_PORT:
			case NET_STATE_SEND_PAYLOAD_SIZE:
			case NET_STATE_SEND_PAYLOAD:
			case NET_STATE_SEND_CHECKSUM:
				// received a byte during sending phase. On half-duplex TWI,
				// that's an error. Probably should abort the send, but
				// the easiest implementation is just to quietly let it
				// complete.
				LOGF((logfp, "Received byte %02x during send. Ignoring.", byte));
				break;
		}
		net_heartbeat(net);
		goto func_exit;
	}

	if (hal_twi_ready_to_send())
	{
		SendSlot *ss = NULL;
		SendSlotPtrQueue_peek(SendQueue(net), &ss);
		
		switch (net->state)
		{
			case NET_STATE_RESYNC:
			case NET_STATE_RECV_PORT:
			case NET_STATE_RECV_PAYLOAD_SIZE:
			case NET_STATE_RECV_PAYLOAD:
			case NET_STATE_RECV_CHECKSUM:
				// someone else owns the medium right now. Wait.
				// Fall through to timeout check.
				goto func_timeout_check;
			case NET_STATE_IDLE:
				//LOGF((logfp, "Start send\n"));
				if (SendSlotPtrQueue_length(SendQueue(net))==0)
				{
					// nothing to send.
					goto func_exit;
				}
				hal_twi_send_byte(NET_SYNC);
				net->state = NET_STATE_SEND_PORT;
				break;
			case NET_STATE_SEND_PORT:
			{
				assert(ss!=NULL);
				hal_twi_send_byte(ss->msg->dest_port);
				net->state = NET_STATE_SEND_PAYLOAD_SIZE;
				break;
			}
			case NET_STATE_SEND_PAYLOAD_SIZE:
			{
				assert(ss!=NULL);
				net_init_checksum(&net->checksum);
				hal_twi_send_byte(ss->msg->message_size);
				net->payload_remaining = ss->msg->message_size;
				net_sent_payload(net);
				break;
			}
			case NET_STATE_SEND_PAYLOAD:
			{
				assert(ss!=NULL);
				uint8_t byte = ss->msg->data[ss->msg->message_size - net->payload_remaining];
				hal_twi_send_byte(byte);
				net_update_checksum(&net->checksum, byte);
				net->payload_remaining -= 1;
				net_sent_payload(net);
				break;
			}
			case NET_STATE_SEND_CHECKSUM:
			{
				hal_twi_send_byte(net->checksum);
				SendSlot *ss2 = NULL;
				r_bool rc = SendSlotPtrQueue_pop(SendQueue(net), &ss2);
				assert(rc);
				assert(ss2==ss);
				ss->sending = FALSE;
				ss->func(ss);
				net->state = NET_STATE_IDLE;
				break;
			}
		}
		//LOGF((logfp, "Net delivers a byte\n"));
		net_heartbeat(net);
		goto func_exit;
	}

func_timeout_check:
	if (net_timeout(net))
	{
		if (net->state!=NET_STATE_IDLE)
		{
			LOGF((logfp, "timeout! leaving state %d\n", net->state));
		}
		net->state = NET_STATE_IDLE;
	}
	//LOGF((logfp, "Net does nada\n"));

func_exit:
	//LOGF((logfp, "Net state %d\n", net->state));
	{}
}

void net_timer_update(NetworkTimer *network_timer)
{
	net_update(network_timer->net);
	schedule_us(NET_TIMEOUT, (Activation*) network_timer);
}

void net_got_payload(Network *net)
{
	if (net->payload_remaining == 0)
	{
		net->state = NET_STATE_RECV_CHECKSUM;
	}
	else
	{
		net->state = NET_STATE_RECV_PAYLOAD;
	}
}

void net_sent_payload(Network *net)
{
	if (net->payload_remaining == 0)
	{
		net->state = NET_STATE_SEND_CHECKSUM;
	}
	else
	{
		net->state = NET_STATE_SEND_PAYLOAD;
	}
}

void net_init_checksum(uint8_t *cksum)
{
	*cksum = 0x67;
}

void net_update_checksum(uint8_t *cksum, uint8_t byte)
{
	*cksum = (*cksum)*131 + byte;
}

void net_heartbeat(Network *net)
{
	net->last_activity = clock_time_us();
}

r_bool net_timeout(Network *net)
{
	return later_than(clock_time_us(), net->last_activity + NET_TIMEOUT);
}
