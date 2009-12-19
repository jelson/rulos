#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "rocket.h"
#include "uart.h"
#include "sim.h"


typedef struct {
	ActivationFunc func;
	Time last_run;
	BoardBuffer bbuf;
	uint32_t secs;
} ChaseClockActivation_t;

void update_chase_clock(ChaseClockActivation_t *cc);

void init_chase_clock(ChaseClockActivation_t *cc)
{
	cc->func = (ActivationFunc) update_chase_clock;
	cc->last_run = clock_time_us();
	cc->secs = 0;

	board_buffer_init(&cc->bbuf);
	board_buffer_push(&cc->bbuf, 0);

	schedule_us((Time) 1, (Activation*) cc);
}

void update_chase_clock(ChaseClockActivation_t *cc)
{
	cc->last_run += (Time) 1000000;
	schedule_absolute(cc->last_run, (Activation*) cc);

	cc->secs += 1;

	char buf[9];
	int_to_string2(buf, 4, 1, cc->secs);
	strcpy(buf+4, "SECS");
	ascii_to_bitmap_str(cc->bbuf.buffer, 8, buf);
	board_buffer_draw(&cc->bbuf);
}

int main()
{
	heap_init();
	util_init();
	hal_init(bc_chaseclock);
	board_buffer_module_init();

	// start clock with 10 msec resolution
	init_clock((Time) 10000, TIMER1);

	// initialize our internal state
	ChaseClockActivation_t cca;
	init_chase_clock(&cca);

	cpumon_main_loop();
	return 0;
}
