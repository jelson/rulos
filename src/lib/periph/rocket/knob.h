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

#pragma once

#include <inttypes.h>
#include "periph/7seg_panel/cursor.h"
#include "periph/7seg_panel/region.h"
#include "periph/input_controller/focus.h"

typedef struct {
  UIEventHandlerFunc func;
  const char **msgs;
  uint8_t len;
  uint8_t selected;
  RowRegion region;
  CursorAct cursor;
  UIEventHandler *notify;
} Knob;

void knob_init(Knob *knob, RowRegion region, const char **msgs, uint8_t len,
               UIEventHandler *notify, FocusManager *fa, const char *label);

void knob_set_value(Knob *knob, uint8_t value);
