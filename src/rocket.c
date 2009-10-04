#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "rocket.h"
#include "clock.h"
#include "util.h"
#include "display_boardid.h"
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


/************************************************************************************/
/************************************************************************************/

#if BOARDID
int main()
{
	init_util();
	hal_init();

#if 0
	int i;

	while (1){
		for (i = 0; i < 8; i++) {
			program_segment(0, 0, i, 0);
			_delay_ms(1000);
		}
		for (i = 0; i < 8; i++) {
			program_segment(0, 0, i, 1);
			_delay_ms(1000);
		}
	}
#endif

	clock_init();
	board_buffer_module_init();

	boardid_init();
	cpumon_main_loop();
	return 0;
}
#endif


int main()
{
	init_util();
	hal_init();

	clock_init();
	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase

	//install_handler(ADC, adc_handler);

	board_buffer_module_init();

	FocusManager fa;
	focus_init(&fa);

	InputControllerAct ia;
	input_controller_init(&ia, (UIEventHandler*) &fa);

	LabeledDisplayHandler ldh;
	labeled_display_init(&ldh, 0, &fa);

	DScrollMsgAct da1;
	dscrlmsg_init(&da1, 2, "x", 0);	// overwritted by idle display
	
	IdleDisplayAct idisp;
	idle_display_init(&idisp, &da1, &cpumon);

	char buf[50], *p;
	strcpy(buf, "calib_spin ");
	p = buf+strlen(buf);
	p+=int_to_string(p, 0, 0, cpumon.calibration_spin_counts);
	strcpy(p, " interval ");
	p = buf+strlen(buf);
	p+=int_to_string(p, 0, 0, cpumon.calibration_interval);
	strcpy(p, "  ");

	DScrollMsgAct da2;
	dscrlmsg_init(&da2, 3, buf, 200);	// overwritted by idle display

/*
	// scroll our ascii set.
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
	dscrlmsg_init(&da2, 3, buf, 200);

#if !MCUatmega8
	NumericInputAct ni;
	BoardBuffer bbuf;
	board_buffer_init(&bbuf);
	board_buffer_push(&bbuf, 4);
	RowRegion region = { &bbuf, 3, 4 };

	numeric_input_init(&ni, region, NULL, &fa, "numeric");
	*/

/*
	Calculator calc;
	calculator_init(&calc, 4, &fa);
*/

/*
	DCompassAct dc;
	dcompass_init(&dc, 4, &fa);

*/

/*
	DDockAct ddock;
	ddock_init(&ddock, 0, &fa);

	DGratuitousGraph dgg;
	dgg_init(&dgg, 5, "volts", 5000);
*/
#if !MCUatmega8
	Launch launch;
	launch_init(&launch, 4);
#endif


	cpumon_main_loop();

	return 0;
}

