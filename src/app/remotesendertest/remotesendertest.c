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
	BoardBuffer *board_buffer;
	int i;
} sendAct_t;

void sendMessage(sendAct_t *sa)
{
	sa->i = (sa->i + 1) % 1000;

	char buf[9];
	strcpy(buf, "HiYa    ");
	debug_itoha(&buf[4], sa->i);
    ascii_to_bitmap_str(sa->board_buffer->buffer, 8, buf);
    board_buffer_draw(sa->board_buffer);

	schedule_us(INTER_MESSAGE_DELAY_US, (ActivationFuncPtr) sendMessage, sa);
}


int main()
{
    hal_init();
	hal_init_rocketpanel(bc_default);
    init_clock(10000, TIMER1); 

	BoardBuffer board_buffer;
    sendAct_t sa = { &board_buffer, 0 };

    RemoteBBufSend remote_bbuf_send;
    install_remote_bbuf_send(&remote_bbuf_send);

    board_buffer_module_init();
    board_buffer_init(&board_buffer DBG_BBUF_LABEL("main"));
    board_buffer_push(&board_buffer, 0);
    
	sendMessage(&sa);

	CpumonAct cpumon;
	cpumon_init(&cpumon);
	cpumon_main_loop();
	assert(FALSE);
}
