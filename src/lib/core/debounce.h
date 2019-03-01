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

#include "core/time.h"
#include "core/util.h"

typedef struct {
  r_bool is_pressed;
  Time refrac_time_us;
  Time next_valid_push_time;
} DebouncedButton_t;

void debounce_button_init(DebouncedButton_t* b, Time refrac_time_us);

// Pass in whether the raw (un-debounced) button is in the pushed state.
// Returns true if the debounced button was just pushed.
r_bool debounce_button(DebouncedButton_t* b, r_bool raw_is_down);
