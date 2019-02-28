/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
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
