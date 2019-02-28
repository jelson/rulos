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

#include "periph/7seg_panel/board_buffer.h"
#include "periph/input_controller/focus.h"
#include "periph/rocket/calculator_decoration.h"
#include "periph/rocket/display_scroll_msg.h"
#include "periph/rocket/knob.h"
#include "periph/rocket/numeric_input.h"

typedef struct s_calculator {
  UIEventHandlerFunc func;
  BoardBuffer bbuf[2];
  BoardBuffer *btable[2];
  NumericInputAct operands[2];
  Knob op;
  NumericInputAct result;
  FocusManager focus;

  struct s_decoration_timeout {
    Time last_activity;
    FetchCalcDecorationValuesIfc *fetchDecorationValuesObj;
  } decorationTimeout;
} Calculator;

void calculator_init(Calculator *calc, int board0, FocusManager *fa,
                     FetchCalcDecorationValuesIfc *fetchDecorationValuesObj);
