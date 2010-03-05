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
#include "remote_uie.h"


/************************************************************************************/
/************************************************************************************/


int main()
{
	heap_init();
	util_init();
	hal_init(bc_rocket1);
	hal_init_keypad();
	init_clock(10000, TIMER1);

	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase

	Network network;
	init_network(&network, ROCKET1_ADDR);

	//install_handler(ADC, adc_handler);

	board_buffer_module_init();

/*
	FocusManager fa;
	focus_init(&fa);
*/

	RemoteKeyboardSend rks;
	init_remote_keyboard_send(&rks, &network, ROCKET_ADDR, REMOTE_KEYBOARD_PORT);

/*
	RemoteBBufRecv rbr;
	init_remote_bbuf_recv(&rbr, &network);
*/

	DAER daer;
	daer_init(&daer, 0, ((Time)5)<<20);
	
	DThrusterGraph dtg;
	dtg_init(&dtg, 1, &network);

	Calculator calc;
	calculator_init(&calc, 2, NULL,
		(FetchCalcDecorationValuesIfc*) &daer.decoration_ifc);

	CascadedInputInjector cii;
	init_cascaded_input_injector(&cii, (UIEventHandler*) &calc.focus, &rks.forwardLocalStrokes);
	RemoteKeyboardRecv rkr;
	init_remote_keyboard_recv(&rkr, &network, (InputInjectorIfc*) &cii, REMOTE_SUBFOCUS_PORT0);

	InputPollerAct ip;
#define LOCALTEST 0
#if LOCALTEST
	input_poller_init(&ip, (InputInjectorIfc*) &cii);
#else
	input_poller_init(&ip, &rks.forwardLocalStrokes);
#endif

	ScreenBlankerListener sbl;
	init_screenblanker_listener(&sbl, &network, bc_rocket1);

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

