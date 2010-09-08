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

#ifndef _remote_bbuf_h
#define _remote_bbuf_h

#include "rocket.h"
#include "network.h"
#include "board_buffer.h"
#include "network_ports.h"

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
