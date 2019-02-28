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

#include "core/util.h"

#ifndef _BV
#define _BV(x) (1 << (x))
#endif

#define JOYSTICK_STATE_UP _BV(0)
#define JOYSTICK_STATE_DOWN _BV(1)
#define JOYSTICK_STATE_LEFT _BV(2)
#define JOYSTICK_STATE_RIGHT _BV(3)
#define JOYSTICK_STATE_TRIGGER _BV(4)
#define JOYSTICK_STATE_DISCONNECTED _BV(5)
typedef struct {
  // ADC channel numbers.  Should be initialized by caller before
  // joystick_init is called.
  uint8_t x_adc_channel;
  uint8_t y_adc_channel;

  // X and Y positions (from -100 to 100) of joystick, and a state
  // bitvector, valid after joystick_poll is called.
  int8_t x_pos;
  int8_t y_pos;
  uint8_t state;
} JoystickState_t;

void joystick_init(JoystickState_t *js);
void joystick_poll(JoystickState_t *js);
