#ifndef clock_h
#define clock_h

#include <inttypes.h>
#include "heap.h"
void clock_init();
uint32_t clock_time();	// current time since boot, ms

void schedule(int offset_ms, Activation *act);
void scheduler_run();

#endif // clock_h
