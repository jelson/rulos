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
#include "network.h"
#include "sim.h"
#include "audio_driver.h"
#include "audio_server.h"

#define AUDIOTEST 0

#if AUDIOTEST
typedef struct {
	ActivationFunc func;
	AudioDriver *ad;
} AudioTest;

void at_update(AudioTest *at)
{
	schedule_us(1000000, (Activation*) at);
	ad_skip_to_clip(at->ad, 0, deadbeef_rand()%8, sound_silence);
}

void init_audio_test(AudioTest *at, AudioDriver *ad)
{
	at->func = (ActivationFunc) at_update;
	at->ad = ad;
	schedule_us(1, (Activation*)at);
}
#endif

int main()
{
	heap_init();
	util_init();
	hal_init(bc_audioboard);
	init_clock(1000, TIMER1);

	AudioDriver ad;
	init_audio_driver(&ad);

	Network network;
	init_network(&network);

	AudioServer as;
	init_audio_server(&as, &ad, &network, 0);

	ad_skip_to_clip(&ad, 0, sound_apollo_11_countdown, sound_apollo_11_countdown);
#if AUDIOTEST
	AudioTest at;
	init_audio_test(&at, &ad);
#endif

	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase

	board_buffer_module_init();


	cpumon_main_loop();

	return 0;
}

