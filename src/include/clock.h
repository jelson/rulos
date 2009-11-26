#ifndef __clock_h__
#define __clock_h__

#ifndef __rocket_h__
# error Please include rocket.h instead of this file
#endif


uint8_t later_than(Time a, Time b);

void init_clock(Time interval_us, uint8_t timer_id);

extern Time _stale_time_us;	

// cheap but only precise to one jiffy
static inline Time clock_time_us() { return _stale_time_us; }

// expensive but more accurate
Time precise_clock_time_us(); 

void schedule_us(Time offset_us, Activation *act);
	// schedule in the future. (asserts us>0)
void schedule_now(Activation *act);
	// Be very careful with schedule_now -- it can result in an infinite
	// loop if you schedule yourself for now repeatedly.
void schedule_absolute(Time at_time, Activation *act);

#define Exp2Time(v)	(((Time)1)<<(v))
//#define schedule_ms(ms,act) { schedule_us(ms*1000, act); }

void scheduler_run_once();

void spin_counter_increment();
uint32_t read_spin_counter();

#endif // clock_h
