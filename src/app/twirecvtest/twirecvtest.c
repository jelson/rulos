#include "rocket.h"

static void recv_func(RecvSlot *recvSlot, uint8_t payload_size)
{
	char buf[payload_size+1];
	SSBitmap bm[8];
	memcpy(buf, recvSlot->msg->data, payload_size);
	buf[payload_size] = '\0';
	LOGF((logfp, "Got network message [%d bytes]: '%s'\n", payload_size, buf));

	// display the first 8 chars to the leds
	ascii_to_bitmap_str(bm, min(payload_size, 8), buf);
	program_board(2, bm);
}


void test_netstack()
{
	Network net;
	char data[20];
	RecvSlot recvSlot;

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
	hal_init(bc_rocket1);

	test_netstack();
	return 0;
}
