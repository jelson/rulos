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

#ifndef _NETWORK_H
#define _NETWORK_H


#ifndef __rocket_h__
# error Please include rocket.h instead of this file
#endif


#define PORT_NONE 255
#define SLOT_NONE 255
#define MAX_LISTENERS 10
#define SEND_QUEUE_SIZE 4
#define NET_MAX_PAYLOAD_SIZE 10

typedef uint8_t Port;

// Wire format for packets.  Format:
// Dest port
// Checksum
// Payload_Len
// [Payload_Len * uint8_t]
typedef struct s_message {
	Port dest_port;
	uint8_t checksum;
	uint8_t payload_len;
	char data[0];
} Message, *MessagePtr;

struct s_recv_slot;
typedef void (*RecvCompleteFunc)(struct s_recv_slot *recv_slot, uint8_t payload_size);
typedef struct s_recv_slot {
	RecvCompleteFunc func;
	Port port;
	Message *msg;	// must contain enough space
	uint8_t payload_capacity;
	uint8_t msg_occupied;	// message in use by app; can't refill now.
	void *user_data; // pointer can be used for user functions
} RecvSlot;

struct s_send_slot;
typedef void (*SendCompleteFunc)(struct s_send_slot *send_slot);
typedef struct s_send_slot {
	SendCompleteFunc func;
	Addr dest_addr;
	Message *msg;
	r_bool sending;
} SendSlot, *SendSlotPtr;

#include "queue.mh"
QUEUE_DECLARE(SendSlotPtr)


typedef struct s_network {
	RecvSlot *recvSlots[MAX_LISTENERS];
	uint8_t sendQueue_storage[sizeof(SendSlotPtrQueue)+sizeof(SendSlotPtr)*SEND_QUEUE_SIZE];
	union {
		char MediaRecvSlotStorage
			[sizeof(MediaRecvSlot) + sizeof(Message) + NET_MAX_PAYLOAD_SIZE];
		MediaRecvSlot mrs;
	};
	MediaStateIfc *media;
} Network;



// Public API
void init_network(Network *net, MediaStateIfc *media);
void net_bind_receiver(Network *net, RecvSlot *recvSlot);
r_bool net_send_message(Network *net, SendSlot *sendSlot);

//////////////////////////////////////////////////////////////////////////////

void init_twi_network(Network *network, Addr local_addr);

#endif // _NETWORK_H
