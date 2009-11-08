#ifndef _remote_bbuf_h
#define _remote_bbuf_h

#include "rocket.h"
#include "network.h"
#include "board_buffer.h"

#define REMOTE_BBUF_PORT	(0x13)
#define REMOTE_BBUF_NUM_BOARDS	NUM_BOARDS

typedef struct {
	SSBitmap buf[NUM_DIGITS];
	uint8_t index;
} BBufMessage;

typedef struct s_remote_bbuf_send {
	ActivationFunc func;
	Network *network;
	uint8_t send_msg_alloc[sizeof(Message)+sizeof(BBufMessage)];
	SendSlot sendSlot;
	struct s_remote_bbuf_send *send_this;
	SSBitmap offscreen[REMOTE_BBUF_NUM_BOARDS][NUM_DIGITS];
	r_bool changed[REMOTE_BBUF_NUM_BOARDS];
	uint8_t last_index;
} RemoteBBufSend;

void init_remote_bbuf_send(RemoteBBufSend *rbs, Network *network);
void send_remote_bbuf(RemoteBBufSend *rbs, SSBitmap *bm, uint8_t index, uint8_t mask);

typedef struct s_remote_bbuf_recv {
	uint8_t recv_msg_alloc[sizeof(Message)+sizeof(BBufMessage)];
	RecvSlot recvSlot;
} RemoteBBufRecv;

void init_remote_bbuf_recv(RemoteBBufRecv *rbr, Network *network);


#endif // _remote_bbuf_h
