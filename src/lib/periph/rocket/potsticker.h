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

#include "periph/input_controller/input_controller.h"
#include "periph/rocket/rocket.h"

typedef struct s_potsticker {
  uint8_t adc_channel;
  InputInjectorIfc *ifi;
  uint8_t detents;
  int8_t hysteresis;
  Keystroke fwd;
  Keystroke back;

  int8_t last_digital_value;
} PotSticker;

void init_potsticker(PotSticker *ps, uint8_t adc_channel, InputInjectorIfc *ifi,
                     uint8_t detents, Keystroke fwd, Keystroke back);
