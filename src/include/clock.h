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

#ifndef __clock_h__
#define __clock_h__

#ifndef __rocket_h__
# error Please include rocket.h instead of this file
#endif


uint8_t later_than(Time a, Time b);

void init_clock(Time interval_us, uint8_t timer_id);

extern Time _last_scheduler_run_us;
extern volatile Time _interrupt_driven_jiffy_clock_us; // must have lock to read!

// very cheap but only precise to one jiffy.  this should be used my
// most functions.
static inline Time clock_time_us() { return _last_scheduler_run_us; }

static inline Time get_interrupt_driven_jiffy_clock() {
	Time retval;
	uint8_t old_interrupts = hal_start_atomic();
	retval = _interrupt_driven_jiffy_clock_us;
	hal_end_atomic(old_interrupts);
	return retval;
}



// expensive but more accurate
Time precise_clock_time_us(); 

void schedule_us(Time offset_us, Activation *act);
	// schedule in the future. (asserts us>0)
void schedule_now(Activation *act);
	// Be very careful with schedule_now -- it can result in an infinite
	// loop if you schedule yourself for now repeatedly (because the clock
	// never advances past now until the queue empties).
void schedule_absolute(Time at_time, Activation *act);

#define Exp2Time(v)	(((Time)1)<<(v))
//#define schedule_ms(ms,act) { schedule_us(ms*1000, act); }

void scheduler_run_once();

void spin_counter_increment();
uint32_t read_spin_counter();

#endif // clock_h
