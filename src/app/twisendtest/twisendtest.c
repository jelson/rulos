#include "rocket.h"


void test_without_netstack()
{
	char inbuf[200];
	TWIRecvSlot *trs = (TWIRecvSlot *) inbuf;
	trs->occupied = FALSE;
	trs->capacity = sizeof(inbuf) - sizeof(TWIRecvSlot);
	
	hal_twi_init(0x8, trs);
	hal_twi_send(0x1, "hello", 5, NULL, NULL);
	while(1) {}

    //sprintf(buf, "hi%3d", i);
    //ascii_to_bitmap_str(bm, 8, buf);
    //program_board(2, bm);
  //    hal_delay_ms(1000);

}

typedef struct {
	ActivationFunc f;
	Network *net;
	SendSlot *sendSlot;
	int i;
} sendAct_t;

void sendMessage(sendAct_t *sa)
{
	sa->i++;
	sprintf(sa->sendSlot->msg->data, "HELLO%3d", sa->i);
	sa->sendSlot->msg->payload_len = strlen(sa->sendSlot->msg->data);

	if (sa->sendSlot->sending)
	{
		LOGF((logfp, "twisendtest: can't send message: outgoing buffer is busy\n"));
	}
	else
	{
		net_send_message(sa->net, sa->sendSlot);
		LOGF((logfp, "sent message\n"));
	}

	schedule_us(1000000, (Activation *) sa);
	
}

void test_netstack()
{
	Network net;
	char data[20];
	SendSlot sendSlot;

	init_clock(10000, TIMER1);

	sendSlot.func = NULL;
	sendSlot.dest_addr = 0x1;
	sendSlot.msg = (Message *) data;
	sendSlot.msg->dest_port = 0x88;
	sendSlot.sending = FALSE;

	sendAct_t sa;
	sa.f = (ActivationFunc) sendMessage;
	sa.net = &net;
	sa.sendSlot = &sendSlot;
	sa.i = 0;
	
	init_network(&net, 0x5);
	schedule_now((Activation *) &sa);

	CpumonAct cpumon;
	cpumon_init(&cpumon);
	cpumon_main_loop();
}

int main()
{
	heap_init();
	util_init();
	hal_init(bc_rocket0);

	// test_without_netstack();
	test_netstack();

	return 0;
}
