#ifndef clock_h
#define clock_h

#include <inttypes.h>
#include "heap.h"
void clock_init();

typedef uint32_t Time;

// Current as of last scheduler execution; cheap to evaluate
// we don't bother offering externally the more-expensive version that reads
// the real current time with an atomic op, because it would only differ
// if our code sucked so much that we were consuming > 1 jiffy for one
// continuation.

// not sure why "inline" defn doesn't work. Meh.
//inline Time clock_time() { return _stale_time_ms; }
extern Time _stale_time_ms;	
#define clock_time()	(_stale_time_ms)

void schedule(int offset_ms, Activation *act);
void scheduler_run_once();

void spin_counter_increment();
uint32_t read_spin_counter();

#endif // clock_h
