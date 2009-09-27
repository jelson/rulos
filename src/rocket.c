#include <inttypes.h>

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

/************************************************************************************/
/************************************************************************************/

int main()
{
	init_util();
	hal_init();

	clock_init();
	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase

	/* display self-test */
	program_matrix(0xff);
	delay_ms(100);
	program_matrix(0);

	//install_handler(ADC, adc_handler);

	board_buffer_module_init();

	FocusManager fa;
	focus_init(&fa);

	InputControllerAct ia;
	input_controller_init(&ia, (UIEventHandler*) &fa);

	LabeledDisplayHandler ldh;
	labeled_display_init(&ldh, 0, &fa);

	DScrollMsgAct da1;
	dscrlmsg_init(&da1, 5, " ", 0);
	
	IdleDisplayAct idisp;
	idle_display_init(&idisp, &da1, &cpumon);

/*
	DScrollMsgAct da2;
	char buf[129-32];
	{
		int i;
		for (i=0; i<128-32; i++)
		{
			buf[i] = i+32;
		}
		buf[i] = '\0';
	}
	dscrlmsg_init(&da2, 3, buf, 75);
*/

	NumericInputAct ni;
	BoardBuffer bbuf;
	board_buffer_init(&bbuf);
	board_buffer_push(&bbuf, 3);
	RowRegion region = { &bbuf, 3, 4 };
	numeric_input_init(&ni, region, NULL, &fa);

/*
	Calculator calc;
	calculator_init(&calc, 4, &fa);
*/

/*
	DCompassAct dc;
	dcompass_init(&dc, 4, &fa);

	DGratuitousGraph dgg;
	dgg_init(&dgg, 5, "volts", 5000);
*/

/*
	DDockAct ddock;
	ddock_init(&ddock, 0, &fa);
*/

	cpumon_main_loop();

	return 0;
}

