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
#include "quadknob.h"


/************************************************************************************/
/************************************************************************************/


int main()
{
	util_init();
	hal_init(bc_rocket1);
	hal_init_keypad();
	init_clock(10000, TIMER1);

	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase

	Network network;
	init_twi_network(&network, ROCKET1_ADDR);

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

#ifndef SIM
	// IO pins aren't defined for SIM. Clunky. Could simulate them?
	IOPinDef q0pin0 = PINDEF(GPIO_C0);
	IOPinDef q0pin1 = PINDEF(GPIO_C1);
	QuadKnob q0;
	init_quadknob(&q0, &rks.forwardLocalStrokes, &q0pin0, &q0pin1, 'r', 's');

	IOPinDef q1pin0 = PINDEF(GPIO_C2);
	IOPinDef q1pin1 = PINDEF(GPIO_C3);
	QuadKnob q1;
	init_quadknob(&q1, &rks.forwardLocalStrokes, &q1pin0, &q1pin1, 'a', 'b');
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

#if MEASURE_CPU_FOR_RULOS_PAPER
	DScrollMsgAct dsm;
	dscrlmsg_init(&dsm, 1, "bong", 100);
	IdleDisplayAct idle;
	idle_display_init(&idle, &dsm, &cpumon);
#endif // MEASURE_CPU_FOR_RULOS_PAPER

	cpumon_main_loop();

	return 0;
}

