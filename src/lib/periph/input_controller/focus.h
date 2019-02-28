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

#include "core/rulos.h"
#include "periph/7seg_panel/cursor.h"
#include "periph/7seg_panel/region.h"
#include "periph/input_controller/ui_event_handler_ifc.h"

struct s_focus_act;

typedef struct {
  RectRegion rr;
  UIEventHandler *handler;
  const char *label;
} FocusChild;
#define NUM_CHILDREN 8

typedef struct s_focus_act {
  UIEventHandlerFunc func;
  CursorAct cursor;
  FocusChild children[NUM_CHILDREN];
  uint8_t children_size;
  uint8_t selectedChild;
  uint8_t focusedChild;
} FocusManager;

void focus_init(FocusManager *act);
void focus_register(FocusManager *act, UIEventHandler *handler, RectRegion rr,
                    const char *label);
r_bool focus_is_active(FocusManager *act);
