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

#include "periph/rocket/rocket.h"
#include "periph/rocket/thruster_protocol.h"

#define MAX_IDLE_HANDLERS 4
#define IDLE_PERIOD (((Time)1000000) * 60 * 5)  // five minutes.
//#define IDLE_PERIOD (((Time)1000000)*3)			// 20 seconds
//(test)
// NB can't make it more than half the clock rollover time, or
// later_than will screw up.  I think that's about 10 min.

typedef struct s_idle_act {
  UIEventHandler *handlers[MAX_IDLE_HANDLERS];
  uint8_t num_handlers;
  r_bool nowactive;
  Time last_touch;
} IdleAct;

void init_idle(IdleAct *idle);
void idle_add_handler(IdleAct *idle, UIEventHandler *handler);
void idle_touch(IdleAct *idle);
void idle_thruster_listener_func(IdleAct *idle);
