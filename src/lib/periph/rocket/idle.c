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

#include "periph/rocket/idle.h"

void idle_update(IdleAct *idle);
void idle_set_active(IdleAct *idle, r_bool nowactive);
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

void idle_set_active(IdleAct *idle, r_bool nowactive) {
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
