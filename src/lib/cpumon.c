#include "rocket.h"
#include "cpumon.h"

#ifdef TIMING_DEBUG
# include "hardware.h"
#endif

uint8_t _run_main_loop;

void cpumon_act(CpumonAct *act);

void cpumon_init(CpumonAct *act)
{
	// read initial time in an activation, so it's on a clock boundary
	// we get called early, before other activations (other processes)
	// start getting queued.
	act->func = (ActivationFunc) cpumon_act;
	act->phase = cpumon_phase_align;
	act->num_calibrations = 3;
	schedule_us(1, (Activation *) act);

	cpumon_main_loop();
	// exits once due to message from phase_measure; next call runs forever.
}

void cpumon_main_loop()
{
	Time now;

	now = get_interrupt_driven_jiffy_clock();

	_run_main_loop = TRUE;
	while (_run_main_loop)
	{
		_last_scheduler_run_us = now;

		scheduler_run_once(now);

		do {
#ifdef TIMING_DEBUG
			gpio_clr(GPIO_D4);
#endif
			hal_idle();
#ifdef TIMING_DEBUG
			gpio_set(GPIO_D4);
#endif
			now = get_interrupt_driven_jiffy_clock();
		} while (_last_scheduler_run_us == now);

		spin_counter_increment();
	}
}

void cpumon_act(CpumonAct *act)
{
	uint32_t spins = read_spin_counter();
	Time time = clock_time_us();

	switch (act->phase)
	{
		case cpumon_phase_align:
		{
			act->phase = cpumon_phase_measure;
			schedule_us(100000, (Activation *) act);
			break;
		}
		case cpumon_phase_measure:
		{
			act->calibration_spin_counts = spins - act->last_spin_counter;
			act->calibration_interval = time - act->last_time;
			act->phase = cpumon_phase_periodic_sample;
				// we won't get run again until real main loop starts.
//			LOGF((logfp, "calib %d/%dms\n",
//				act->calibration_spin_counts, act->calibration_interval));
			act->num_calibrations--;
			if (act->num_calibrations>0)
			{
				act->phase = cpumon_phase_align;
				schedule_us(1, (Activation *) act);
			}
			else
			{
				schedule_us(1000000, (Activation *) act);
				// signal that we're done with cpumon_init()'s invocation of
				// cpumon_main_loop; let rocket.c's main initialization
				// continue until it's called for real (and forEVVEEEEERRRR).
				_run_main_loop = FALSE;
			}
			break;
		}
		case cpumon_phase_periodic_sample:
		{
			act->sample_spin_counts = spins - act->last_spin_counter;
			act->sample_interval = time - act->last_time;
			/*
			LOGF((logfp, "calib %d/%dms sample %d/%dms idle %d%%\n",
				act->calibration_spin_counts, act->calibration_interval,
				act->sample_spin_counts, act->sample_interval,
				cpumon_get_idle_percentage(act)));
			*/
			schedule_us(1000000, (Activation *) act);
			break;
		}
	}
	act->last_spin_counter = spins;
	act->last_time = time;
}

uint16_t cpumon_get_idle_percentage(CpumonAct *act)
{
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
