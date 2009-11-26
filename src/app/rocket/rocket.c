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
#include "display_aer.h"
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
#include "remote_keyboard.h"
#include "remote_bbuf.h"
#include "remote_uie.h"
#include "control_panel.h"


/************************************************************************************/
/************************************************************************************/


int main()
{
	heap_init();
	util_init();
	hal_init(bc_rocket0);
	init_clock(10000, TIMER1);

	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase

	//install_handler(ADC, adc_handler);

	board_buffer_module_init();

	Network network;
	init_network(&network);

	DRTCAct dr;
	drtc_init(&dr, 0, clock_time_us()+20000000);

	LunarDistance ld;
	lunar_distance_init(&ld, 1, 2);

#if 0	// my ill-fated attempt to run the calc code here and remote the display
	RemoteBBufSend rbs;
	init_remote_bbuf_send(&rbs, &network);
	install_remote_bbuf_send(&rbs);

	DAER daer;
	daer_init(&daer, NUM_BOARDS+0, ((Time)5)<<20);
	
	Calculator calc;
	calculator_init(&calc, NUM_BOARDS+2, &fa,
		(FetchCalcDecorationValuesIfc*) &daer.decoration_ifc);
#endif

	AudioClient audio_client;
	init_audio_client(&audio_client, &network);

	ThrusterUpdate *thrusterUpdate[3] =
	{
		NULL,
		NULL,
		NULL
	};
	HPAM hpam;
	init_hpam(&hpam, 3, thrusterUpdate);

	ControlPanel cp;
	init_control_panel(&cp, 4, 1, &network, &hpam, &audio_client);
	cp.ccl.launch.main_rtc = &dr;
	cp.ccl.launch.lunar_distance = &ld;

	// Both local poller and remote receiver inject keyboard events.
	InputPollerAct ip;
	input_poller_init(&ip, (InputInjectorIfc*) &cp.direct_injector);

	RemoteKeyboardRecv rkr;
	init_remote_keyboard_recv(&rkr, &network, (InputInjectorIfc*) &cp.direct_injector, REMOTE_KEYBOARD_PORT);

	ThrusterSendNetwork tsn;
	init_thruster_send_network(&tsn, &network);
	thrusterUpdate[0] = (ThrusterUpdate*) &tsn;
	thrusterUpdate[1] = (ThrusterUpdate*) &cp.ccdock.dock.thrusterUpdate;


#define THRUSTER_X_CHAN	3
#define THRUSTER_Y_CHAN	2
	ThrusterState_t ts;
	thrusters_init(&ts, 3, THRUSTER_X_CHAN, THRUSTER_Y_CHAN, &hpam);
// Stuff that can go on matrix display:
/*
	Launch launch;
	launch_init(&launch, 4, &fa);

	DDockAct ddock;
	ddock_init(&ddock, 4, &fa);
	
	Pong pong;
	pong_init(&pong, 4, &fa);

	RasterBigDigit rdigit;
	raster_big_digit_init(&rdigit, 4);
*/

	cpumon_main_loop();

	return 0;
}

