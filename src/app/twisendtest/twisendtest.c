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


void test_without_netstack()
{
	char inbuf[200];
	MediaRecvSlot *trs = (MediaRecvSlot *) inbuf;
	trs->capacity = sizeof(inbuf) - sizeof(MediaRecvSlot);
	
	MediaStateIfc *media = hal_twi_init(100, 0x8, trs);
	(media->send)(media, 0x1, "hello", 5, NULL, NULL);
	while(1) {}

    //sprintf(buf, "hi%3d", i);
    //ascii_to_bitmap_str(bm, 8, buf);
    //program_board(2, bm);
  //    hal_delay_ms(1000);

}

typedef struct {
	Network *net;
	SendSlot *sendSlot;
	int i;
} sendAct_t;


void board_say(char *s)
{
	SSBitmap bm[8];
	int i;

	ascii_to_bitmap_str(bm, 8, s);

	for (i = 0; i < 8; i++)
		program_board(i, bm);
}

void sendMessage(sendAct_t *sa)
{
	char buf[9];

	sa->i = (sa->i + 1) % 1000;

	strcpy(buf, "sENd    ");
	debug_itoha(&buf[4], sa->i);
	board_say(buf);

	strcpy(sa->sendSlot->msg->data, "HELO");
	debug_itoha(&sa->sendSlot->msg->data[4], sa->i);
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

	schedule_us(1000000, (ActivationFuncPtr) sendMessage, sa);
}


void test_netstack()
{
	Network net;

	init_clock(10000, TIMER1);

	char data[30];
	SendSlot sendSlot;


	sendSlot.func = NULL;
	sendSlot.dest_addr = AUDIO_ADDR;
	sendSlot.msg = (Message *) data;
	sendSlot.msg->dest_port = AUDIO_PORT;
	sendSlot.sending = FALSE;

	init_twi_network(&net, 100, 0x5);

	sendAct_t sa;
	sa.net = &net;
	sa.sendSlot = &sendSlot;
	sa.i = 0;
	
	schedule_us(1000000, (ActivationFuncPtr) sendMessage, &sa);

	CpumonAct cpumon;
	cpumon_init(&cpumon);
	cpumon_main_loop();
	assert(FALSE);
}

int main()
{
	hal_init();

	board_say("  InIt  ");
	// test_without_netstack();
	test_netstack();

	return 0;
}
