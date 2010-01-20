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
#include "autotype.h"
#include "idle.h"
#include "hobbs.h"
#include "screenblanker.h"


/************************************************************************************/
/************************************************************************************/

typedef struct {
	DRTCAct dr;
	Network network;
	LunarDistance ld;
	AudioClient audio_client;
	ThrusterUpdate *thrusterUpdate[4];
	HPAM hpam;
	ControlPanel cp;
	InputPollerAct ip;
	RemoteKeyboardRecv rkr;
	ThrusterSendNetwork tsn;
	ThrusterState_t ts;
	IdleAct idle;
	Hobbs hobbs;
	ScreenBlanker screenblanker;
	ScreenBlankerSender screenblanker_sender;
} Rocket0;

#define THRUSTER_X_CHAN	3

#if defined(BOARD_PCB10)
#define THRUSTER_Y_CHAN	4
#elif defined(BOARD_PCB11)
#define THRUSTER_Y_CHAN	2
#elif SIM
#define THRUSTER_Y_CHAN	2
#endif

void init_rocket0(Rocket0 *r0)
{
	drtc_init(&r0->dr, 0, clock_time_us()+20000000);
	init_network(&r0->network);
	lunar_distance_init(&r0->ld, 1, 2);
	init_audio_client(&r0->audio_client, &r0->network);
	memset(&r0->thrusterUpdate, 0, sizeof(r0->thrusterUpdate));
	init_hpam(&r0->hpam, 7, r0->thrusterUpdate);
	init_idle(&r0->idle);
	thrusters_init(&r0->ts, 7, THRUSTER_X_CHAN, THRUSTER_Y_CHAN, &r0->hpam, &r0->idle);
	//r0->thrusterUpdate[2] = (ThrusterUpdate*) &r0->idle.thrusterListener;

	init_screenblanker(&r0->screenblanker, bc_rocket0, &r0->hpam, &r0->idle);
	init_screenblanker_sender(&r0->screenblanker_sender, &r0->network);
	r0->screenblanker.screenblanker_sender = &r0->screenblanker_sender;

	init_control_panel(&r0->cp, 3, 1, &r0->network, &r0->hpam, &r0->audio_client, &r0->idle, &r0->screenblanker, &r0->ts.joystick_state);
	r0->cp.ccl.launch.main_rtc = &r0->dr;
	r0->cp.ccl.launch.lunar_distance = &r0->ld;
	// Both local poller and remote receiver inject keyboard events.
	input_poller_init(&r0->ip, (InputInjectorIfc*) &r0->cp.direct_injector);
	init_remote_keyboard_recv(&r0->rkr, &r0->network, (InputInjectorIfc*) &r0->cp.direct_injector, REMOTE_KEYBOARD_PORT);

	r0->thrusterUpdate[1] = (ThrusterUpdate*) &r0->cp.ccdock.dock.thrusterUpdate;
	init_thruster_send_network(&r0->tsn, &r0->network);
	r0->thrusterUpdate[0] = (ThrusterUpdate*) &r0->tsn;

	init_hobbs(&r0->hobbs, &r0->hpam, &r0->idle);
}

static Rocket0 rocket0;	// allocate obj in .bss so it's easy to count

int main()
{
	heap_init();
	util_init();
	hal_init(bc_rocket0);
	hal_init_keypad();
	init_clock(10000, TIMER1);

	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase

	board_buffer_module_init();

	init_rocket0(&rocket0);

#define DEBUG_IDLE_BUSY 0
#if DEBUG_IDLE_BUSY
	DScrollMsgAct dsm;
	dscrlmsg_init(&dsm, 2, "bong", 100);
	IdleDisplayAct idle;
	idle_display_init(&idle, &dsm, &cpumon);
#endif // DEBUG_IDLE_BUSY

/*
	Autotype autotype;
	init_autotype(&autotype, (InputInjectorIfc*) &rocket0.cp.direct_injector,
		"000aaaaac004671c", (Time) 1300000);
*/

	cpumon_main_loop();

	return 0;
}

