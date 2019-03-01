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

#include <stdint.h>

#include "core/time.h"

typedef void (*ActivationFuncPtr)(void *data);

typedef struct {
  ActivationFuncPtr func;
  void *data;
} ActivationRecord;

typedef struct {
  Time key;
  ActivationRecord activation;
} HeapEntry;

#ifndef SCHEDULER_CAPACITY
#define SCHEDULER_CAPACITY 32
#endif

typedef struct {
  HeapEntry heap[SCHEDULER_CAPACITY];
  uint8_t heap_count;
} Heap;

void heap_init(Heap *heap);
// NB: got an old ref to heap_init() (no args) in your main()?
// Just discard it; it's now handeld by init_clock().
uint8_t heap_insert(Heap *heap, Time key, ActivationFuncPtr func, void *data);
int heap_peek(Heap *heap, /*out*/ Time *key, /*out*/ ActivationRecord *act);
/* rc nonzero => heap empty */
void heap_pop(Heap *heap);
