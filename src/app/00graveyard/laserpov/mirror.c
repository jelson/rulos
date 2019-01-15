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

#include <stdio.h>

#include "core/hardware.h"
#include "mirror.h"

void mirror_handler();
static MirrorHandler *theMirror = NULL;

void mirror_init(MirrorHandler *mirror) {
  mirror->last_interrupt = 0;
  mirror->period = 0;
  assert(theMirror == NULL);
  theMirror = mirror;
  sensor_interrupt_register_handler(mirror_handler);
}

void mirror_handler() {
  Time now = clock_time_us();
  theMirror->period = now - theMirror->last_interrupt;
  theMirror->last_interrupt = now;
}
