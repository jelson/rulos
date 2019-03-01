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

#include "core/clock.h"
#include "periph/7seg_panel/board_buffer.h"
#include "periph/input_controller/focus.h"
#include "periph/rocket/drift_anim.h"

struct s_dcompassact;

typedef struct {
  UIEventHandlerFunc func;
  struct s_dcompassact *act;
} DCompassHandler;

typedef struct s_dcompassact {
  BoardBuffer bbuf;
  BoardBuffer *btable;  // needed for RectRegion
  DCompassHandler handler;
  uint8_t focused;
  DriftAnim drift;
  uint32_t last_impulse_time;
} DCompassAct;

void dcompass_init(DCompassAct *act, uint8_t board, FocusManager *focus);
