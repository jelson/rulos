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

#pragma once

#include <inttypes.h>

#include "core/time.h"

typedef enum {
	cpumon_phase_align,
	cpumon_phase_measure,
	cpumon_phase_periodic_sample,
} CpumonPhase;

typedef struct s_cpumon_act {
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
