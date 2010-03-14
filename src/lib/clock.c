#include <inttypes.h>

#include "rocket.h"

void schedule_us_internal(Time offset_us, Activation *act);

Time _rtc_interval_us;
Time _real_time_since_boot_us;
Time _stale_time_us;	// current as of last scheduler execution; cheap to evaluate
uint32_t _spin_counter;

// Returns true if a > b using rollover math, assuming 32-bit signed time values
uint8_t later_than(Time a, Time b)
{
	// the subtraction will roll over too
	return a - b > 0;

	// this took forever to puzzle out and was originally a
	// complicated set of conditionals
}


void clock_handler()
{
	// NB we assume this runs in interrupt context and is hence
	// automatically atomic.
	_real_time_since_boot_us += _rtc_interval_us;
}

void init_clock(Time interval_us, uint8_t timer_id)
{
	// Initialize the clock to 20 seconds before rollover time so that 
	// rollover bugs happen quickly during testing
	_real_time_since_boot_us = (((uint32_t) 1) << 31) - ((uint32_t) 20000000);
	_stale_time_us = _real_time_since_boot_us;
	_rtc_interval_us = hal_start_clock_us(interval_us, clock_handler, timer_id);
	_spin_counter = 0;
}

void schedule_us(Time offset_us, Activation *act)
{
	// never schedule anything for "now", or we might stick the scheduler
	// in a loop at "now".
	assert(offset_us > 0);
	schedule_us_internal(offset_us, act);
}

void schedule_now(Activation *act)
{
	schedule_us_internal(0, act);
}

void schedule_us_internal(Time offset_us, Activation *act)
{
	schedule_absolute(clock_time_us() + offset_us, act);
}

void schedule_absolute(Time at_time, Activation *act)
{
	//LOGF((logfp, "scheduling act %08x func %08x\n", (int) act, (int) act->func));

	uint8_t old_interrupts;
	old_interrupts = hal_start_atomic();
	heap_insert(at_time, act);
	hal_end_atomic(old_interrupts);
}

// this is the expensive one, with a lock
Time precise_clock_time_us()
{
	uint16_t milliintervals;
	uint8_t old_interrupts;

	old_interrupts = hal_start_atomic();
	milliintervals = hal_elapsed_milliintervals();
	_stale_time_us = _real_time_since_boot_us;
	hal_end_atomic(old_interrupts);

	_stale_time_us += ((_rtc_interval_us * milliintervals) / 1000);
	return _stale_time_us;
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
	Time now = precise_clock_time_us();

	while (1)	// run until nothing to do for this time
	{
		Time due_time;
		Activation *act;
		int rc;
		uint8_t old_interrupts;

		r_bool valid = FALSE;

		old_interrupts = hal_start_atomic();
		rc = heap_peek(&due_time, &act);
		if (!rc && !later_than(due_time, now))
		{
			valid = TRUE;
			heap_pop();
		}
		hal_end_atomic(old_interrupts);

		if (!valid)
		{
			break;
		}

		//LOGF((logfp, "popping act %08x func %08x\n", (uint32_t) act, (uint32_t) act->func));

		act->func(act);
		//LOGF((logfp, "returned act %08x func %08x\n", (uint32_t) act, (uint32_t) act->func));
	}
}
