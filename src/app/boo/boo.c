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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "rocket.h"
#include "clock.h"
#include "util.h"
#include "display_controller.h"
#include "display_rtc.h"
#include "display_scroll_msg.h"
#include "display_compass.h"
#include "focus.h"
#include "labeled_display.h"
#include "display_docking.h"
#include "display_gratuitous_graph.h"
#include "numeric_input.h"
#include "input_controller.h"
#include "calculator.h"
#include "hal.h"
#include "cpumon.h"
#include "idle_display.h"
#include "sequencer.h"
#include "rasters.h"
#include "pong.h"
#include "lunar_distance.h"


typedef struct {
	Time time;
	const char *msg;
} BooRec;
BooRec boos[] = {
	{ 5000000, "" },
	{ 0250000, "  boo!" },
	{ 5000000, "" },
	{ 1000000, "Come see" },
	{ 1000000, "our new " },
	{ 1000000, " rocket!" },
	{ 0, NULL },
};
BooRec *booPtr = boos;
BoardBuffer bbuf;

void boofunc(void *f)
{
	booPtr += 1;
	if (booPtr->msg == NULL) { booPtr = boos; }
	memset(bbuf.buffer, 0, 8);
	ascii_to_bitmap_str(bbuf.buffer, 8, booPtr->msg);
	LOGF((logfp, "msg: %s", booPtr->msg));
	schedule_us(booPtr->time, (ActivationFuncPtr) boofunc, NULL);
	board_buffer_draw(&bbuf);
}

/************************************************************************************/
/************************************************************************************/


int main()
{
	hal_init();
	hal_init_rocketpanel(bc_chaseclock);
	init_clock(10000, TIMER1);

	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase

	//install_handler(ADC, adc_handler);

	board_buffer_module_init();

/*
	FocusManager fa;
	focus_init(&fa);
*/

	InputPollerAct ia;
	input_poller_init(&ia, NULL);

	board_buffer_init(&bbuf);
	board_buffer_push(&bbuf, 0);
	schedule_us(1, boofunc, NULL);

	DRTCAct dr;
	drtc_init(&dr, 1, clock_time_us()+20000000);

	cpumon_main_loop();

	return 0;
}

