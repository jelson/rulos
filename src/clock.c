#include <inttypes.h>

#include "clock.h"
#include "util.h"
#include "heap.h"
#include "rocket.h"
#include "hal.h"

#define RTC_INTERVAL_MS	10
Time _real_time_since_boot_ms;
Time _stale_time_ms;	// current as of last scheduler execution; cheap to evaluate
uint32_t _spin_counter;

void clock_handler()
{
	// NB we assume this runs in interrupt context and is hence
	// automatically atomic.
	_real_time_since_boot_ms += RTC_INTERVAL_MS;
}

void clock_init()
{
	_real_time_since_boot_ms = 0;
	start_clock_ms(RTC_INTERVAL_MS, clock_handler);
	heap_init();
	_spin_counter = 0;
}

void schedule(int offset_ms, Activation *act)
{
	//LOGF((logfp, "scheduling act %08x\n", (int) act));

	// never schedule anything for "now", or we might stick the scheduler
	// in a loop at "now".
	assert(offset_ms > 0);

	heap_insert(clock_time() + offset_ms, act);
}

Time _read_clock()	// this is the expensive one, with a lock
{
	hal_start_atomic();
	_stale_time_ms = _real_time_since_boot_ms;
	hal_end_atomic();
	return _stale_time_ms;
}

void spin_counter_increment()
{
	_spin_counter += 1;
}

uint32_t read_spin_counter()
{
	return _spin_counter;
}

void scheduler_run_once()
{
	Time now = _read_clock();

	while (1)	// run until nothing to do for this time
	{
		uint32_t due_time;
		Activation *act;
		int rc;
		rc = heap_peek(&due_time, &act);

		if (rc!=0) { break; }
			// no work to do at all

		if (due_time > now) { break; }
			// no work to do now

		//LOGF((logfp, "popping act %08x func %08x\n", (uint32_t) act, (uint32_t) act->func));
		heap_pop();
		act->func(act);
		//LOGF((logfp, "returned act %08x func %08x\n", (uint32_t) act, (uint32_t) act->func));
	}
}
