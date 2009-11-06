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
#include "display_thrusters.h"
#include "network.h"


/************************************************************************************/
/************************************************************************************/


int main()
{
#if SIM
	sim_twi_set_instance(0);
	sim_configure_tree(tree0);
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

	DRTCAct dr;
	drtc_init(&dr, 0, clock_time_us()+20000000);

	LunarDistance ld;
	lunar_distance_init(&ld, 1, 2);

	Network network;
	init_network(&network);

#define THRUSTER_X_CHAN	3
#define THRUSTER_Y_CHAN	2
	ThrusterState_t ts;
	thrusters_init(&ts, 3, THRUSTER_X_CHAN, THRUSTER_Y_CHAN, &network);


#if !MCUatmega8
// Stuff that can go on matrix display:
/*
	Launch launch;
	launch_init(&launch, 4, &fa);

	DDockAct ddock;
	ddock_init(&ddock, 4, &fa);
	
	Pong pong;
	pong_init(&pong, 4, &fa);
*/

	RasterBigDigit rdigit;
	raster_big_digit_init(&rdigit, 4);
#endif


	cpumon_main_loop();

	return 0;
}

