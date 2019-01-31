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

#include <inttypes.h>
#include <stdlib.h>

#include "core/clock.h"
#include "core/logging.h"

#ifdef TIMING_DEBUG
#include "core/hardware.h"
#endif

#ifndef LOG_CLOCK_STATS
#define LOG_CLOCK_STATS 0
#endif

#ifndef SCHEDULER_NOW_QUEUE_CAPACITY
#define SCHEDULER_NOW_QUEUE_CAPACITY 4
#endif

void schedule_us_internal(Time offset_us, ActivationFuncPtr func, void *data);

// usec between timer interrupts
Time g_rtc_interval_us;

// Clock updated by timer interrupt.  Should not be accessed without a
// lock since it is updated at interrupt time.
volatile Time g_interrupt_driven_jiffy_clock_us;

// last time the scheduler ran.  Can be accessed safely without a lock.
Time g_last_scheduler_run_us;

uint32_t g_spin_counter;

extern volatile uint8_t run_scheduler_now;

typedef struct {
  Heap heap;
  ActivationRecord now_queue[SCHEDULER_NOW_QUEUE_CAPACITY];
  uint8_t now_queue_size;

#if LOG_CLOCK_STATS
  // Scheduler stats collection
  Time min_period;
  ActivationFuncPtr min_period_func;
  Time max_period;
  ActivationFuncPtr max_period_func;
  uint8_t peak_heap;
  uint8_t peak_now;
#endif
} SchedulerState_t;
static SchedulerState_t sched_state;

#if LOG_CLOCK_STATS
static void reset_stats() {
  sched_state.min_period = 0;
  sched_state.min_period_func = NULL;
  sched_state.max_period = 0;
  sched_state.max_period_func = NULL;
  sched_state.peak_heap = 0;
  sched_state.peak_now = 0;
}
#endif

void clock_log_stats() {
#if LOG_CLOCK_STATS
  LOG("peak %d scheduled, range %d (%p) to %d (%p); now peak %d",
      sched_state.peak_heap, sched_state.min_period,
      sched_state.min_period_func, sched_state.max_period,
      sched_state.max_period_func, sched_state.peak_now);

  reset_stats();
#endif
}

void clock_handler(void *data) {
#ifdef TIMING_DEBUG
#error TIMING_DEBUG is on. Just wanted you to know.
  gpio_set(GPIO_D5);
#endif
  // NB we assume this runs in interrupt context and is hence
  // automatically atomic.
  g_interrupt_driven_jiffy_clock_us += g_rtc_interval_us;
  run_scheduler_now = TRUE;
#ifdef TIMING_DEBUG
  gpio_clr(GPIO_D5);
#endif
}

void init_clock(Time interval_us, uint8_t timer_id) {
  heap_init(&sched_state.heap);
  sched_state.now_queue_size = 0;

#if LOG_CLOCK_STATS
  reset_stats();
#endif

#ifdef TIMING_DEBUG
  gpio_make_output(GPIO_D4);
  gpio_make_output(GPIO_D5);
  gpio_make_output(GPIO_D6);
#endif
  // Initialize the clock to 20 seconds before rollover time so that
  // rollover bugs happen quickly during testing
  g_interrupt_driven_jiffy_clock_us = (Time)INT32_MAX - 20000000;
  g_rtc_interval_us =
      hal_start_clock_us(interval_us, clock_handler, NULL, timer_id);
  g_spin_counter = 0;
}

void schedule_us(Time offset_us, ActivationFuncPtr func, void *data) {
  // never schedule anything for "now", or we might stick the scheduler
  // in a loop at "now".
  assert(offset_us > 0);
  schedule_us_internal(offset_us, func, data);
}

void scheduler_insert(Time key, ActivationFuncPtr func, void *data) {
#if LOG_CLOCK_STATS
  uint8_t heap_count =
#endif
      heap_insert(&sched_state.heap, key, func, data);

#if LOG_CLOCK_STATS
  if (heap_count > sched_state.peak_heap) {
    sched_state.peak_heap = heap_count;
  }
#endif
}

void schedule_now(ActivationFuncPtr func, void *data) {
  rulos_irq_state_t old_interrupts = hal_start_atomic();
  if (sched_state.now_queue_size < SCHEDULER_NOW_QUEUE_CAPACITY) {
    sched_state.now_queue[sched_state.now_queue_size].func = func;
    sched_state.now_queue[sched_state.now_queue_size].data = data;
    sched_state.now_queue_size++;

#if LOG_CLOCK_STATS
    if (sched_state.now_queue_size > sched_state.peak_now) {
      sched_state.peak_now = sched_state.now_queue_size;
    }
#endif
  } else {
    scheduler_insert(clock_time_us(), func, data);
  }
  hal_end_atomic(old_interrupts);
  run_scheduler_now = TRUE;
}

void schedule_us_internal(Time offset_us, ActivationFuncPtr func, void *data) {
#if LOG_CLOCK_STATS
  if (sched_state.min_period == 0 || sched_state.min_period > offset_us) {
    sched_state.min_period = offset_us;
    sched_state.min_period_func = func;
  }
  if (sched_state.max_period < offset_us) {
    sched_state.max_period = offset_us;
    sched_state.max_period_func = func;
  }
#endif

  schedule_absolute(clock_time_us() + offset_us, func, data);
}

void schedule_absolute(Time at_time, ActivationFuncPtr func, void *data) {
  // LOG("scheduling act %08x func %08x", (int) act, (int) act->func);

#ifdef TIMING_DEBUG
  gpio_set(GPIO_D6);
#endif
  rulos_irq_state_t old_interrupts = hal_start_atomic();
  scheduler_insert(at_time, func, data);
  hal_end_atomic(old_interrupts);
#ifdef TIMING_DEBUG
  gpio_clr(GPIO_D6);
#endif
}

// this is the expensive one, with a lock
Time precise_clock_time_us() {
  uint16_t milliintervals;
  Time t;

  rulos_irq_state_t old_interrupts = hal_start_atomic();
  milliintervals = hal_elapsed_milliintervals();
  t = g_interrupt_driven_jiffy_clock_us;
  hal_end_atomic(old_interrupts);

  t += ((g_rtc_interval_us * milliintervals) / 1000);
  return t;
}

void spin_counter_increment() { g_spin_counter += 1; }

uint32_t read_spin_counter() { return g_spin_counter; }

void scheduler_run_once() {
  // update output of clock_time_us()
  g_last_scheduler_run_us = get_interrupt_driven_jiffy_clock();

  while (1)  // run until nothing to do for this time
  {
    Time due_time;
    ActivationRecord act;
    int rc;

    r_bool valid = FALSE;

#ifdef TIMING_DEBUG
    gpio_set(GPIO_D6);
#endif
    rulos_irq_state_t old_interrupts = hal_start_atomic();
    if (sched_state.now_queue_size > 0) {
      act = sched_state.now_queue[0];
      uint8_t nqi;
      for (nqi = 1; nqi < sched_state.now_queue_size; nqi++) {
        sched_state.now_queue[nqi - 1] = sched_state.now_queue[nqi];
      }
      sched_state.now_queue_size -= 1;
      valid = TRUE;
    } else {
      rc = heap_peek(&sched_state.heap, &due_time, &act);
      if (!rc && !later_than(due_time, g_last_scheduler_run_us)) {
        valid = TRUE;
        heap_pop(&sched_state.heap);
      }
    }
    hal_end_atomic(old_interrupts);
#ifdef TIMING_DEBUG
    gpio_clr(GPIO_D6);
#endif

    if (!valid) {
      break;
    }

    // LOG("popping act %08x func %08x", (uint32_t) act, (uint32_t)
    // act->func);

    act.func(act.data);
    // LOG("returned act %08x func %08x", (uint32_t) act, (uint32_t)
    // act->func);
  }
}
