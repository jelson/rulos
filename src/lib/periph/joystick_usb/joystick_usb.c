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

#include "periph/joystick_usb/joystick_usb.h"
#include "periph/max3421e/periph_thrustmaster.h"

static bool joystick_usb_read(JoystickState_t *js_base) {
  Joystick_USB_t *js = (Joystick_USB_t *)js_base;

  return thrustmaster_read(js->max, js_base);
}

void init_joystick_usb(Joystick_USB_t *js, max3421e_t *max) {
  js->base.state = 0;
  js->base.joystick_reader_func = joystick_usb_read;

  js->max = max;
}
