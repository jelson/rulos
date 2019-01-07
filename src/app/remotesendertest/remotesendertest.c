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

#include <stdio.h>
#include <stdlib.h>

#include "core/rulos.h"
//#include "core/twi.h"
#include "periph/7seg_panel/board_buffer.h"
#include "periph/7seg_panel/7seg_panel.h"

#define INTER_MESSAGE_DELAY_US 125000

typedef struct {
	BoardBuffer *bb0, *bb4;
	int i;
} sendAct_t;

void sendMessage(sendAct_t *sa)
{
	sa->i = (sa->i + 1) % 1000;

	char buf[9];
	strcpy(buf, "HiYa    ");
	debug_itoha(&buf[4], sa->i);

    ascii_to_bitmap_str(sa->bb0->buffer, 8, buf);
    board_buffer_draw(sa->bb0);

    ascii_to_bitmap_str(sa->bb4->buffer, 8, buf);
    board_buffer_draw(sa->bb4);

	schedule_us(INTER_MESSAGE_DELAY_US, (ActivationFuncPtr) sendMessage, sa);
}


int main()
{
    hal_init();
	hal_init_rocketpanel(bc_default);
    init_clock(10000, TIMER1); 

	Network network;
	init_twi_network(&network, 100, 0x5);

	BoardBuffer bb0, bb4;
    sendAct_t sa = { &bb0, &bb4, 0 };

    RemoteBBufSend remote_bbuf_send;

    board_buffer_module_init();
    init_remote_bbuf_send(&remote_bbuf_send, &network);
    install_remote_bbuf_send(&remote_bbuf_send);
    board_buffer_init(&bb0 DBG_BBUF_LABEL("main"));
    board_buffer_push(&bb0, 0);
    board_buffer_init(&bb4 DBG_BBUF_LABEL("remote"));
    board_buffer_push(&bb4, 4);
    
	sendMessage(&sa);

	CpumonAct cpumon;
	cpumon_init(&cpumon);
	cpumon_main_loop();
	assert(FALSE);
}