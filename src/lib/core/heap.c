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

#include <stdlib.h>

#include "core/clock.h"
#include "core/heap.h"
#include "core/logging.h"

void SchedulerHeap::Init() {
  heap_count_ = 0;
}

void SchedulerHeap::swap(int off0, int off1) {
  SchedulerHeapEntry tmp = heap_[off0];
  heap_[off0] = heap_[off1];
  heap_[off1] = tmp;
}

void SchedulerHeap::bubble(int ptr) {
  while (ptr > 0) {
    int parent = ptr / 2;
    if (heap_[parent].time.later_than(heap_[ptr].time)) {
      return;
    }  // already correct

    swap(parent, ptr);
    ptr = parent;
  }
}

uint8_t SchedulerHeap::Insert(Time time, Task *task) {
  assert(heap_count_ < SCHEDULER_CAPACITY);  // heap overflow
  heap_[heap_count_].time = time;
  heap_[heap_count_].task = task;
  bubble(heap_count_);
  heap_count_++;

  return heap_count_;

#if PRINT_ALL_SCHEDULES
  LOG("heap_count %d this task %08" PRIxPTR " period %d", heap_count_,
      task, time - _last_scheduler_run_us);
#endif
}

int SchedulerHeap::Peek(Time *time /*out*/, Task **task /* out */) {
  if (heap_count_ == 0) {
    return -1;
  }
  *time = heap_[0].time;
  *task = heap_[0].task;
  return 0;
}

void SchedulerHeap::Pop() {
  assert(heap_count_ > 0);  // heap underflow
  heap_[0] = heap_[heap_count_ - 1];
  heap_count_--;

  /* down-heap */
  int ptr = 0;
  while (true) {
    int c0 = ptr * 2;
    int c1 = c0 + 1;
    int candidate = ptr;
    if (c0 < heap_count_ && heap_[c0].time.later_than(heap_[candidate].time)) {
      candidate = c0;
    }
    if (c1 < heap_count_ && heap_[c1].time.later_than(heap_[candidate].time)) {
      candidate = c1;
    }
    if (candidate == ptr) {
      return;  // down-heaped as far as it goes.
    }
    swap(ptr, candidate);
    ptr = candidate;
  }
}
