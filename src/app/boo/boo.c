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
	char *msg;
} BooRec;
BooRec boos[] = {
	{ 5000000, "" },
	{ 0250000, "  boo!" },
	{ 5000000, "" },
	{ 1000000, "Come see" },
	{ 1000000, "our new " },
	{ 1000000, "rocket! " },
	{ 0, NULL },
};
BooRec *booPtr = boos;
BoardBuffer bbuf;

typedef struct {
	ActivationFunc func;
} BooAct;
BooAct booAct;

void boofunc(void *f)
{
	booPtr += 1;
	if (booPtr->msg == NULL) { booPtr = boos; }
	memset(bbuf.buffer, 0, 8);
	ascii_to_bitmap_str(bbuf.buffer, 8, booPtr->msg);
	LOGF((logfp, "msg: %s", booPtr->msg));
	schedule_us(booPtr->time, (Activation*) &booAct);
	board_buffer_draw(&bbuf);
}

/************************************************************************************/
/************************************************************************************/


int main()
{
	heap_init();
	util_init();
	hal_init();
	clock_init(10000);

	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase

	//install_handler(ADC, adc_handler);

	board_buffer_module_init();

	FocusManager fa;
	focus_init(&fa);

	InputControllerAct ia;
	input_controller_init(&ia, (UIEventHandler*) &fa);

	board_buffer_init(&bbuf);
	board_buffer_push(&bbuf, 0);
	booAct.func = (ActivationFunc) boofunc;
	schedule_us(1, (Activation*) &booAct);

	DRTCAct dr;
	drtc_init(&dr, 1, clock_time_us()+20000000);

	cpumon_main_loop();

	return 0;
}

