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

#include "core/util.h"
#include "periph/joystick/joystick.h"

typedef struct {
  // "base class" state
  JoystickState_t base;

  // ADC channel numbers.  Should be initialized by caller before
  // joystick_init is called.
  uint8_t x_adc_channel;
  uint8_t y_adc_channel;
} Joystick_ADC_t;

void init_joystick_adc(Joystick_ADC_t *js, uint8_t x_chan, uint8_t y_chan);
