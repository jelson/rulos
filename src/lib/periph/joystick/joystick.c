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

#include "periph/joystick/joystick.h"

#define ACTUATION_THRESHOLD 50
#define RELEASE_THRESHOLD   30

void joystick_poll(JoystickState_t *js) {
  assert(js->joystick_reader_func != NULL);

  // If read returns false, there's no joystick present.
  if (!js->joystick_reader_func(js)) {
    js->state = JOYSTICK_STATE_DISCONNECTED;
    return;
  }

  // Indicate joystick is present.
  js->state &= ~(JOYSTICK_STATE_DISCONNECTED);

  // Threshold the left/right state.
  if (js->x_pos < -ACTUATION_THRESHOLD) {
    js->state |= JOYSTICK_STATE_LEFT;
  }
  if (js->x_pos > -RELEASE_THRESHOLD) {
    js->state &= ~(JOYSTICK_STATE_LEFT);
  }
  if (js->x_pos > ACTUATION_THRESHOLD) {
    js->state |= JOYSTICK_STATE_RIGHT;
  }
  if (js->x_pos < RELEASE_THRESHOLD) {
    js->state &= ~(JOYSTICK_STATE_RIGHT);
  }

  // Threshold the up/down state.
  if (js->y_pos > ACTUATION_THRESHOLD) {
    js->state |= JOYSTICK_STATE_UP;
  }
  if (js->y_pos < RELEASE_THRESHOLD) {
    js->state &= ~(JOYSTICK_STATE_UP);
  }
  if (js->y_pos < -ACTUATION_THRESHOLD) {
    js->state |= JOYSTICK_STATE_DOWN;
  }
  if (js->y_pos > -RELEASE_THRESHOLD) {
    js->state &= ~(JOYSTICK_STATE_DOWN);
  }
}
