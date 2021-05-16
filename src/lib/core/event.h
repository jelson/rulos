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

#include "core/heap.h"
#include "core/util.h"

typedef struct {
  bool auto_reset;
  bool signaled;
  ActivationFuncPtr waiter_func;
  void *waiter_data;
} Event;

void event_init(Event *evt, bool auto_reset);
void event_signal(Event *evt);
void event_reset(Event *evt);
void event_wait(Event *evt, ActivationFuncPtr func, void *data);

static inline bool event_is_signaled(Event *evt) { return evt->signaled; }
