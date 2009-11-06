#ifndef _NETWORK_H
#define _NETWORK_H

#include "rocket.h"

#define PORT_NONE 255
#define SLOT_NONE 255
#define MAX_LISTENERS 10
#define SEND_QUEUE_SIZE 4
#define NET_TIMEOUT	(100000)	/* 0.1 sec timout */

#define NET_SYNC 	(0xf3)

typedef uint8_t Port;

typedef struct s_message {
	Port dest_port;
	uint8_t message_size;
	uint8_t data[0];
} Message, *MessagePtr;

struct s_recv_slot;
typedef void (*RecvCompleteFunc)(struct s_recv_slot *recv_slot);
typedef struct s_recv_slot {
	RecvCompleteFunc func;
	Port port;
	uint8_t payload_capacity;
	uint8_t msg_occupied;	// message in use by app; can't refill now.
	Message *msg;	// must contain enough space
} RecvSlot;

struct s_send_slot;
typedef void (*SendCompleteFunc)(struct s_send_slot *send_slot);
typedef struct s_send_slot {
	SendCompleteFunc func;
	Message *msg;
	r_bool sending;
} SendSlot, *SendSlotPtr;

#include "queue.mh"
QUEUE_DECLARE(SendSlotPtr)

typedef enum {
	NET_STATE_IDLE,
	NET_STATE_RECV_PORT,
	NET_STATE_RECV_PAYLOAD_SIZE,
	NET_STATE_RECV_PAYLOAD,
	NET_STATE_RECV_CHECKSUM,
	NET_STATE_RESYNC,	//	misaligned; look for sync byte.
	NET_STATE_SEND_PORT,
	NET_STATE_SEND_PAYLOAD_SIZE,
	NET_STATE_SEND_PAYLOAD,
	NET_STATE_SEND_CHECKSUM,
} NetState;

struct s_network_timer;

typedef struct s_network_timer {
	ActivationFunc func;
	struct s_network *net;
} NetworkTimer;

typedef struct s_network {
	ActivationFunc func;
	RecvSlot *recvSlots[MAX_LISTENERS];
	uint8_t sendQueue_storage[sizeof(SendSlotPtrQueue)+sizeof(MessagePtr)*SEND_QUEUE_SIZE];
	NetState state;
	uint8_t slot;	// RecvSlot index for message being received
	uint8_t payload_remaining;
	uint8_t checksum;
	Time last_activity;
	NetworkTimer network_timer;
} Network;


void init_network(Network *net);
void net_bind_receiver(Network *net, RecvSlot *recvSlot);
r_bool net_send_message(Network *net, SendSlot *sendSlot);

#endif // _NETWORK_H
