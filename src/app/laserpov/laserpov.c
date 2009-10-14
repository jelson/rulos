#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "rocket.h"
#include "clock.h"
#include "hal.h"
#include "cpumon.h"
#include "mirror.h"
#include "pov.h"
#include "input_controller.h"


int main()
{
	heap_init();
	util_init();
	hal_init();
	clock_init(300);

	InputControllerAct ia;
	input_controller_init(&ia, NULL);

	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase

	MirrorHandler mirror;
	mirror_init(&mirror);

	PovHandler pov;
	pov_init(&pov, &mirror, 0, 1);

	board_buffer_module_init();

	cpumon_main_loop();

	return 0;
}

