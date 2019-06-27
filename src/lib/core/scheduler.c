/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
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

#include <array>

#include "core/scheduler.h"
#include "core/task.h"
#include "core/heap.h"

void Scheduler::Scheduler() {
  now_queue_size_ = 0;

#if LOG_SCHEDULER_STATS
  ResetStats();
#endif
}

void Scheduler::ScheduleIn(Interval interval, Task *task) {
}

void Scheduler::ScheduleNow(Task *task) {
}

void Scheduler::RunNow() {
  run_scheduler_now_ = true;
}

void Scheduler::RunForever() {
#ifdef TIMING_DEBUG_PIN
  gpio_make_output(TIMING_DEBUG_PIN);
#endif

  while (true) {
    run_scheduler_now_ = false;

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

void schedule_us(Time offset_us, ActivationFuncPtr func, void *data) {
  // never schedule anything for "now", or we might stick the scheduler
  // in a loop at "now".
  assert(offset_us > 0);
  schedule_us_internal(offset_us, func, data);
}

void Scheduler::AddTaskToHeap(Time time, Task *task) {
  uint8_t heap_count = heap_.Insert(time, task);
  (void) heap_count;
  
#if LOG_CLOCK_STATS
  if (heap_count > peak_heap_) {
    peak_heap_ = heap_count;
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

void schedule_absolute(Time at_time, Task* task) {
  // LOG("scheduling act %08x func %08x", (int) act, (int) act->func);
  rulos_irq_state_t old_interrupts = hal_start_atomic();
  AddTaskToHeap(at_time, task);
  hal_end_atomic(old_interrupts);
}


void Scheduler::RunOnce() {
  Time curr_time = Clock::GetTime();

  // Keep running tasks until there's nothing left to do at the current time.
  while (true) {
    Task* task = NULL;

    rulos_irq_state_t old_interrupts = hal_start_atomic();
    if (sched_state.now_queue_size > 0) {
      task = sched_state.now_queue[0];
      for (uint8_t nqi = 1; nqi < sched_state.now_queue_size; nqi++) {
        sched_state.now_queue[nqi - 1] = sched_state.now_queue[nqi];
      }
      sched_state.now_queue_size--;
      valid = TRUE;
    } else {
      Time due_time;
      int rc = heap_peek(&sched_state.heap, &due_time, &task);
      if (!rc && !curr_time.later_than(due_time)) {
        heap_pop(&sched_state.heap);
      } else {
	task = NULL;
      }
    }
    hal_end_atomic(old_interrupts);

    if (task == NULL) {
      break;
    }

    // LOG("popping act %08x func %08x", (uint32_t) act, (uint32_t)
    // act->func);

    task->Run();
    // LOG("returned act %08x func %08x", (uint32_t) act, (uint32_t)
    // act->func);
  }
}

#if LOG_SCHEDULER_STATS
void Scheduler::ResetStats() {
  min_period_ = 0;
  min_period_func_ = NULL;
  max_period_ = 0;
  max_period_func_ = NULL;
  peak_heap_ = 0;
}

void Scheduler::LogStats() {
  LOG("peak %d scheduled, range %d (%p) to %d (%p); now peak %d",
      peak_heap_, min_period_,
      min_period_func_, max_period_,
      max_period_func_, peak_now_);

  ResetStats();
}
#endif // LOG_SCHEDULER_STATS

void spin_counter_increment();
uint32_t read_spin_counter();

