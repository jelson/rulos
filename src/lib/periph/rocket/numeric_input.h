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

#include "periph/rocket/rocket.h"

typedef struct {
  uint16_t mantissa;
  uint8_t neg_exponent;  // base 10
} DecimalFloatingPoint;

uint8_t dfp_draw(DecimalFloatingPoint *dfp, SSBitmap *bm, uint8_t len,
                 uint8_t show_decimal);

struct s_numeric_input_act;
typedef struct {
  UIEventHandlerFunc func;
  struct s_numeric_input_act *act;
} NumericInputHandler;

typedef struct s_numeric_input_act {
  // ActivationFunc func;
  RowRegion region;
  NumericInputHandler handler;
  CursorAct cursor;
  DecimalFloatingPoint old_value;
  DecimalFloatingPoint cur_value;
  uint8_t decimal_present;
  UIEventHandler *notify;
  const char *msg;
} NumericInputAct;

void numeric_input_init(NumericInputAct *act, RowRegion region,
                        UIEventHandler *notify, FocusManager *fa,
                        const char *label);
void numeric_input_set_value(NumericInputAct *act,
                             DecimalFloatingPoint new_value);
void numeric_input_set_msg(NumericInputAct *act, const char *msg);
