#include <inttypes.h>

#include "rocket.h"

#ifdef TIMING_DEBUG
# include "hardware.h"
#endif

void schedule_us_internal(Time offset_us, Activation *act);

// usec between timer interrupts
Time _rtc_interval_us;

// Clock updated by timer interrupt.  Should not be accessed without a
// lock since it is updated at interrupt time.
volatile Time _interrupt_driven_jiffy_clock_us;

// last time the scheduler ran.  Can be accessed safely without a lock.
Time _last_scheduler_run_us;

uint32_t _spin_counter;

// Returns true if a > b using rollover math, assuming 32-bit signed time values
uint8_t later_than(Time a, Time b)
{
	// the subtraction will roll over too
	return a - b > 0;

	// this took forever to puzzle out and was originally a
	// complicated set of conditionals
}


void clock_handler(void *data)
{
#ifdef TIMING_DEBUG
	gpio_set(GPIO_D5);
#endif
	// NB we assume this runs in interrupt context and is hence
	// automatically atomic.
	_interrupt_driven_jiffy_clock_us += _rtc_interval_us;
#ifdef TIMING_DEBUG
	gpio_clr(GPIO_D5);
#endif
}

void init_clock(Time interval_us, uint8_t timer_id)
{
#ifdef TIMING_DEBUG
	gpio_make_output(GPIO_D4);
	gpio_make_output(GPIO_D5);
	gpio_make_output(GPIO_D6);
#endif
	// Initialize the clock to 20 seconds before rollover time so that 
	// rollover bugs happen quickly during testing
	_interrupt_driven_jiffy_clock_us = (((uint32_t) 1) << 31) - ((uint32_t) 20000000);
	_rtc_interval_us = hal_start_clock_us(interval_us, clock_handler, NULL, timer_id);
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

#ifdef TIMING_DEBUG
	gpio_set(GPIO_D6);
#endif
	uint8_t old_interrupts;
	old_interrupts = hal_start_atomic();
	heap_insert(at_time, act);
	hal_end_atomic(old_interrupts);
#ifdef TIMING_DEBUG
	gpio_clr(GPIO_D6);
#endif
}

// this is the expensive one, with a lock
Time precise_clock_time_us()
{
	uint16_t milliintervals;
	uint8_t old_interrupts;
	Time t;

	old_interrupts = hal_start_atomic();
	milliintervals = hal_elapsed_milliintervals();
	t = _interrupt_driven_jiffy_clock_us;
	hal_end_atomic(old_interrupts);

	t += ((_rtc_interval_us * milliintervals) / 1000);
	return t;
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
	Time now = clock_time_us();

	while (1)	// run until nothing to do for this time
	{
		Time due_time;
		Activation *act;
		int rc;
		uint8_t old_interrupts;

		r_bool valid = FALSE;

#ifdef TIMING_DEBUG
		gpio_set(GPIO_D6);
#endif
		old_interrupts = hal_start_atomic();
		rc = heap_peek(&due_time, &act);
		if (!rc && !later_than(due_time, now))
		{
			valid = TRUE;
			heap_pop();
		}
		hal_end_atomic(old_interrupts);
#ifdef TIMING_DEBUG
		gpio_clr(GPIO_D6);
#endif

		if (!valid)
		{
			break;
		}

		//LOGF((logfp, "popping act %08x func %08x\n", (uint32_t) act, (uint32_t) act->func));

		act->func(act);
		//LOGF((logfp, "returned act %08x func %08x\n", (uint32_t) act, (uint32_t) act->func));
	}
}
