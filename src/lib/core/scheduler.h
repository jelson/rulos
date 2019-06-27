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

#pragma once

#ifndef LOG_SCHEDULER_STATS
#define LOG_SCHEDULER_STATS 0
#endif

#ifndef SCHEDULER_NOW_QUEUE_CAPACITY
#define SCHEDULER_NOW_QUEUE_CAPACITY 4
#endif

///////////////////////////////////////////////////////

#include <array>

#include "core/task.h"
#include "core/heap.h"

class Scheduler {
 public:
  // Schedule a task in the future on the heap.
  void ScheduleIn(Interval interval, Task *task);

  // Schedule a task "as soon as possible".
  void ScheduleNow(Task *task);

  // Run the scheduler. Never returns.
  void RunForever();

#if LOG_SCHEDULER_STATS
  // Log statistics about the tasks in the scheduler.
  void LogStats();
#endif
  
 private:
  void RunOnce();

  void AddTaskToHeap(Time time, Task* task);
  
  SchedulerHeap heap_;
  std::array<Task *, SCHEDULER_NOW_QUEUE_CAPACITY> now_queue_;

#if LOG_SCHEDULER_STATS
  // Scheduler stats collection
  Interval min_period_;
  Task *min_period_func_;
  Interval max_period_;
  Task *max_period_func_;
  uint8_t peak_heap_;
  uint8_t peak_now_;
#endif
};

void spin_counter_increment();
uint32_t read_spin_counter();

