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

#include <stdbool.h>
#include <stdlib.h>

#include "core/clock.h"
#include "core/event.h"

extern void syncdebug(uint8_t spaces, char f, uint16_t line);
//#define SYNCDEBUG()	syncdebug(0, 'E', __LINE__)
#define SYNCDEBUG() \
  {}

void event_init(Event *evt, r_bool auto_reset) {
  evt->auto_reset = auto_reset;
  evt->signaled = false;
  evt->waiter_func = NULL;
}

void event_signal(Event *evt) {
  if (evt->waiter_func != NULL) {
    SYNCDEBUG();
    schedule_now(evt->waiter_func, evt->waiter_data);
    evt->waiter_func = NULL;
  } else {
    SYNCDEBUG();
    evt->signaled = true;
  }
}

void event_reset(Event *evt) { evt->signaled = false; }

void event_wait(Event *evt, ActivationFuncPtr func, void *data) {
  if (evt->signaled) {
    SYNCDEBUG();
    schedule_now(func, data);
    if (evt->auto_reset) {
      evt->signaled = false;
    }
  } else {
    SYNCDEBUG();
    evt->waiter_func = func;
    evt->waiter_data = data;
  }
}
