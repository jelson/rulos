#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "rocket.h"


/************************************************************************************/
/************************************************************************************/

typedef struct {
	ActivationFunc update;
	int task;
	int interval;
} RolloverTest_t;

static void update(RolloverTest_t *rt)
{
	LOGF((logfp, "task %d running at jiffy %d\n", rt->task, clock_time_us()));
	schedule_us(rt->interval, (Activation *) rt);
}

int main()
{
	heap_init();
	util_init();
	hal_init();
	clock_init(1000);

	hal_start_atomic();
	extern Time _real_time_since_boot_us;
	_real_time_since_boot_us = (0xffffffff & ~(1 << 31)) - 500647;
	precise_clock_time_us();
	hal_end_atomic();

	RolloverTest_t t1, t2, t3, t4;
	t1.update = t2.update = t3.update = t4.update = (ActivationFunc) update;
	t1.task = 1;
	t1.interval = 10000;

	t2.task = 2;
	t2.interval = 5000;

	t3.task = 3;
	t3.interval = 6000;

	t4.task = 4;
	t4.interval = 3000;

	schedule_us(1, (Activation *) &t1);
   	schedule_us(1, (Activation *) &t2);
	//	schedule_us(1, (Activation *) &t3);
	//	schedule_us(1, (Activation *) &t4);

	cpumon_main_loop();
	return 0;
}

