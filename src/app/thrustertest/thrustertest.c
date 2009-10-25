#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "rocket.h"
#include "joystick.h"
#include "display_thrusters.h"

/************************************************************************************/
/************************************************************************************/


int main()
{
	heap_init();
	util_init();
	hal_init();
	clock_init(10000);
	board_buffer_module_init();

	ThrusterState_t ts;
	thrusters_init(&ts, 7, 3, 2);
	cpumon_main_loop();
	return 0;
}

