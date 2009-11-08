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
#include "display_thruster_graph.h"
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
#include "network.h"
#include "sim.h"
#include "display_aer.h"
#include "remote_keyboard.h"
#include "remote_bbuf.h"


/************************************************************************************/
/************************************************************************************/


int main()
{
#if SIM
	sim_twi_set_instance(1);
	sim_configure_tree(tree1);
#endif //SIM
	heap_init();
	util_init();
	hal_init();
	clock_init(10000);

	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase

	Network network;
	init_network(&network);

	//install_handler(ADC, adc_handler);

	board_buffer_module_init();

	FocusManager fa;
	focus_init(&fa);

	RemoteKeyboardSend rks;
	init_remote_keyboard_send(&rks, &network);

	InputPollerAct ip;
	input_poller_init(&ip, &rks.forwardLocalStrokes);

	RemoteBBufRecv rbr;
	init_remote_bbuf_recv(&rbr, &network);

	DThrusterGraph dtg;
	dtg_init(&dtg, 1, &network);


/*
	DAER daer;
	daer_init(&daer, 0, ((Time)5)<<20);

	Calculator calc;
	calculator_init(&calc, 2, &fa,
		(FetchCalcDecorationValuesIfc*) &daer.decoration_ifc);
*/

	cpumon_main_loop();

	return 0;
}

