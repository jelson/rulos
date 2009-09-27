#ifndef _cpumon_h
#define _cpumon_h

#include "heap.h"
#include "clock.h"

typedef enum {
	cpumon_phase_align,
	cpumon_phase_measure,
	cpumon_phase_periodic_sample,
} CpumonPhase;

typedef struct s_cpumon_act {
	ActivationFunc func;
	CpumonPhase phase;
	uint8_t num_calibrations;
	uint32_t last_spin_counter;
	Time last_time;
	uint32_t calibration_spin_counts;
	Time calibration_interval;
	uint32_t sample_spin_counts;
	Time sample_interval;
} CpumonAct;

void cpumon_init(CpumonAct *act);

// main loop implemented here to guarantee that it's the very same loop
// that runs during calibration phase and real run time.
void cpumon_main_loop();
uint16_t cpumon_get_idle_percentage(CpumonAct *act);

#endif // _cpumon_h
