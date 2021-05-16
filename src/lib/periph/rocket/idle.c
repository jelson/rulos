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

#include "periph/rocket/idle.h"

void idle_update(IdleAct *idle);
void idle_set_active(IdleAct *idle, bool nowactive);
void idle_broadcast(IdleAct *idle);

void init_idle(IdleAct *idle) {
  idle->num_handlers = 0;
  idle->nowactive = FALSE;
  idle->last_touch = clock_time_us();
  idle_set_active(idle, TRUE);
  schedule_us(1, (ActivationFuncPtr)idle_update, idle);
}

#if 0
void idle_thruster_listener_func(IdleAct *idle)
{
	idle_touch(idle);
}
#endif

void idle_add_handler(IdleAct *idle, UIEventHandler *handler) {
  assert(idle->num_handlers < MAX_IDLE_HANDLERS);
  idle->handlers[idle->num_handlers] = handler;
  idle->num_handlers += 1;
  idle_broadcast(idle);
}

void idle_update(IdleAct *idle) {
  schedule_us(1000000, (ActivationFuncPtr)idle_update, idle);
  if (later_than(clock_time_us() - IDLE_PERIOD, idle->last_touch)) {
    // system is idle.
    idle_set_active(idle, FALSE);
    // prevent wrap around by walking an old last-touch time forward
    idle->last_touch = clock_time_us() - IDLE_PERIOD - 1;
  }
}

void idle_set_active(IdleAct *idle, bool nowactive) {
  if (nowactive != idle->nowactive) {
    idle->nowactive = nowactive;
    idle_broadcast(idle);
  }
}

void idle_broadcast(IdleAct *idle) {
  int i;
  UIEvent evt = idle->nowactive ? evt_idle_nowactive : evt_idle_nowidle;

  // LOG("idle_broadcast(%d => %d)", idle->nowactive, evt);

  for (i = 0; i < idle->num_handlers; i++) {
    idle->handlers[i]->func(idle->handlers[i], evt);
  }
}

void idle_touch(IdleAct *idle) {
  idle->last_touch = clock_time_us();
  idle_set_active(idle, TRUE);
}
