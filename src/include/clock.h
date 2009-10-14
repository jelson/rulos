#ifndef __clock_h__
#define __clock_h__

#ifndef __rocket_h__
# error Please include rocket.h instead of this file
#endif

typedef int32_t Time;	// in units of usec

void clock_init(Time interval_us);

// Current as of last scheduler execution; cheap to evaluate
// we don't bother offering externally the more-expensive version that reads
// the real current time with an atomic op, because it would only differ
// if our code sucked so much that we were consuming > 1 jiffy for one
// continuation.

// not sure why "inline" defn doesn't work. Meh.
//inline Time clock_time() { return _stale_time_us; }
extern Time _stale_time_us;	
#define clock_time_us()	(_stale_time_us)

void schedule_us(Time offset_us, Activation *act);
#define Exp2Time(v)	(((Time)1)<<(v))
#define schedule_ms(ms,act) { schedule_us(ms*1000, act); }

void scheduler_run_once();

void spin_counter_increment();
uint32_t read_spin_counter();

#endif // clock_h
