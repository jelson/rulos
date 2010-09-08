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

void board_say(char *s)
{
	SSBitmap bm[8];
	int i;

	ascii_to_bitmap_str(bm, 8, s);

	for (i = 0; i < 8; i++)
		program_board(i, bm);
}

static void recv_func(RecvSlot *recvSlot, uint8_t payload_size)
{
	char buf[payload_size+1];
	memcpy(buf, recvSlot->msg->data, payload_size);
	buf[payload_size] = '\0';
	LOGF((logfp, "Got network message [%d bytes]: '%s'\n", payload_size, buf));

	// display the first 8 chars to the leds
	buf[min(payload_size, 8)] = '\0';
	board_say(buf);
}


void test_netstack()
{
	Network net;
	char data[20];
	RecvSlot recvSlot;

	board_say(" rEAdy  ");

	init_clock(10000, TIMER1);

	recvSlot.func = recv_func;
	recvSlot.port = 0x88;
	recvSlot.msg = (Message *) data;
	recvSlot.payload_capacity = sizeof(data) - sizeof(Message);
	recvSlot.msg_occupied = FALSE;

	init_network(&net, 0x1);
	net_bind_receiver(&net, &recvSlot);

	CpumonAct cpumon;
	cpumon_init(&cpumon);
	cpumon_main_loop();
}

int main()
{
	heap_init();
	util_init();
	hal_init(bc_audioboard);

	test_netstack();
	return 0;
}
