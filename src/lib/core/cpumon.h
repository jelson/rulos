/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
