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
#include "sim.h"


/************************************************************************************/
/************************************************************************************/


int main()
{
#if SIM
	sim_configure_tree(tree1);
#endif //SIM
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

	DScrollMsgAct thruster_actuation_placeholder;
	dscrlmsg_init(&thruster_actuation_placeholder, 0, "aaaeeerr", 0);

/*
	DScrollMsgAct da1;
	dscrlmsg_init(&da1, 0, "x", 0);	// overwritten by idle display
	
	IdleDisplayAct idisp;
	idle_display_init(&idisp, &da1, &cpumon);
*/

#if !MCUatmega8
	DGratuitousGraph dgg;
	dgg_init(&dgg, 1, "pres", 1000000);

	Calculator calc;
	calculator_init(&calc, 2, &fa);
#endif

	cpumon_main_loop();

	return 0;
}

