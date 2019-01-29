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

#include "core/cpumon.h"
#include "core/rulos.h"

#ifdef TIMING_DEBUG_PIN
#include "core/hardware.h"
#endif

uint8_t g_run_main_loop;

void cpumon_act(void *data);

void cpumon_init(CpumonAct *act) {
  // read initial time in an activation, so it's on a clock boundary
  // we get called early, before other activations (other processes)
  // start getting queued.
  act->phase = cpumon_phase_align;
  act->num_calibrations = 3;
  schedule_us(1, cpumon_act, act);

  cpumon_main_loop();
  // exits once due to message from phase_measure; next call runs forever.
}

volatile uint8_t run_scheduler_now = FALSE;

void cpumon_main_loop() {
  g_run_main_loop = TRUE;

#ifdef TIMING_DEBUG_PIN
  gpio_make_output(TIMING_DEBUG_PIN);
#endif

  while (g_run_main_loop) {
    run_scheduler_now = FALSE;

    scheduler_run_once();

    do {
#ifdef TIMING_DEBUG_PIN
      gpio_clr(TIMING_DEBUG_PIN);
#endif
      hal_idle();
#ifdef TIMING_DEBUG_PIN
      gpio_set(TIMING_DEBUG_PIN);
#endif
    } while (!run_scheduler_now);

    spin_counter_increment();
  }
}

void cpumon_act(void *data) {
  CpumonAct *act = (CpumonAct *)data;
  uint32_t spins = read_spin_counter();
  Time time = clock_time_us();

  switch (act->phase) {
    case cpumon_phase_align: {
      act->phase = cpumon_phase_measure;
      schedule_us(100000, cpumon_act, act);
      break;
    }
    case cpumon_phase_measure: {
      act->calibration_spin_counts = spins - act->last_spin_counter;
      act->calibration_interval = time - act->last_time;
      act->phase = cpumon_phase_periodic_sample;
      // we won't get run again until real main loop starts.
      //			LOG("calib %d/%dms\n",
      //				act->calibration_spin_counts,
      // act->calibration_interval);
      act->num_calibrations--;
      if (act->num_calibrations > 0) {
        act->phase = cpumon_phase_align;
        schedule_us(1, cpumon_act, act);
      } else {
        schedule_us(1000000, cpumon_act, act);
        // signal that we're done with cpumon_init()'s invocation of
        // cpumon_main_loop; let rocket.c's main initialization
        // continue until it's called for real (and forEVVEEEEERRRR).
        g_run_main_loop = FALSE;
      }
      break;
    }
    case cpumon_phase_periodic_sample: {
      act->sample_spin_counts = spins - act->last_spin_counter;
      act->sample_interval = time - act->last_time;

#if LOG_CPU_USAGE
      LOG("calib %d/%dms sample %d/%dms idle %d%%\n",
          act->calibration_spin_counts, act->calibration_interval,
          act->sample_spin_counts, act->sample_interval,
          cpumon_get_idle_percentage(act));
#endif
      clock_log_stats();
      schedule_us(1000000, cpumon_act, act);
      break;
    }
  }
  act->last_spin_counter = spins;
  act->last_time = time;
}

uint16_t cpumon_get_idle_percentage(CpumonAct *act) {
  // (sample_spin/sample_time)/(calib_spin/calib_time) * 100
  // = (sample_spin*calib_time*100)/(sample_time*calib_spin)

  // order doodled to avoid int div underflow, int32 overflow
  uint32_t x = act->sample_spin_counts;
  x *= act->calibration_interval;
  x /= act->sample_interval;
  x *= 100;
  x /= act->calibration_spin_counts;
  return x;
}
