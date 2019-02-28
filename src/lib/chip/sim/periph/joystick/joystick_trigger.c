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

#include "core/keystrokes.h"
#include "core/util.h"

// Joystick button here; motion in periph/adc.

r_bool sim_joystick_keystroke_handler(char c);

r_bool g_joystick_trigger_state;

void hal_init_joystick_button() {
  g_joystick_trigger_state = FALSE;
  sim_maybe_init_and_register_keystroke_handler(sim_joystick_keystroke_handler);
}

r_bool hal_read_joystick_button() { return g_joystick_trigger_state; }

r_bool sim_joystick_keystroke_handler(char c) {
  switch (c) {
    case '%':  // button-on
      g_joystick_trigger_state = !g_joystick_trigger_state;
      return true;
    default:
      return false;
  }
}
