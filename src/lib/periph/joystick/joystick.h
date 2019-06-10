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

#include "core/rulos.h"

#ifndef _BV
#define _BV(x) (1 << (x))
#endif

#define JOYSTICK_STATE_UP _BV(0)
#define JOYSTICK_STATE_DOWN _BV(1)
#define JOYSTICK_STATE_LEFT _BV(2)
#define JOYSTICK_STATE_RIGHT _BV(3)
#define JOYSTICK_STATE_TRIGGER _BV(4)
#define JOYSTICK_STATE_DISCONNECTED _BV(5)

struct JoystickState_t_s;

typedef bool (*JoystickReadFunc_t)(struct JoystickState_t_s *js);

typedef struct JoystickState_t_s {
  JoystickReadFunc_t joystick_reader_func;

  // X position from -100 (left) to +100 (right)
  int8_t x_pos;

  // Y position from -100 (down) to +100 (up)
  int8_t y_pos;

  // bitvector with status and thresholded position bits
  uint8_t state;
} JoystickState_t;

void joystick_init(JoystickState_t *js);
void joystick_poll(JoystickState_t *js);
