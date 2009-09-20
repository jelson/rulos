#include <inttypes.h>

#ifdef SIM
#include <sys/time.h>
#include <signal.h>
#endif

#include "clock.h"
#include "util.h"
#include "heap.h"
#include "rocket.h"

#define RTC_INTERVAL_MS	10
uint32_t real_time_since_boot_ms;

void clock_handler()
{
	real_time_since_boot_ms += RTC_INTERVAL_MS;
	scheduler_run();
}

void clock_init()
{
	real_time_since_boot_ms = 0;
	start_clock_ms(RTC_INTERVAL_MS, clock_handler);
	heap_init();
}

int clock_time()
{
	return real_time_since_boot_ms;
}

void schedule(int offset_ms, Activation *act)
{
	//LOGF((logfp, "scheduling act %08x\n", (int) act));
	heap_insert(real_time_since_boot_ms + offset_ms, act);
}

void scheduler_run()
{
	while (1)
	{
		int due_time;
		Activation *act;
		int rc;
		rc = heap_peek(&due_time, &act);

		if (rc!=0) { return; }
			// no work to do at all

		if (due_time > real_time_since_boot_ms) { return; }
			// no work to do now

		//LOGF((logfp, "popping act %08x\n", (int) act));
		heap_pop();
		act->func(act);
	}
}
