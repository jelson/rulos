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

#include <stdint.h>

#include "periph/joystick/joystick.h"
#include "periph/max3421e/max3421e.h"
#include "periph/max3421e/periph_thrustmaster.h"

static int8_t scale_to_99(int8_t scale_to_128) {
  return max(-99, min(99, (((int16_t)scale_to_128) * 100) / 128));
}

bool thrustmaster_read(max3421e_t *max, JoystickState_t *joystate) {
  // find device
  usb_device_t *dev = NULL;

  for (int i = 0; i < MAX_USB_DEVICES; i++) {
    if (max->devices[i].ready && max->devices[i].vid == 0x7b5 &&
        max->devices[i].pid == 0x316) {
      dev = &max->devices[i];
      break;
    }
  }

  if (dev == NULL) {
    return false;
  }

  uint8_t buf[6];
  uint16_t result_len;
  uint8_t result = max3421e_read_data(dev, &dev->endpoints[1], buf, sizeof(buf),
                                      &result_len);

  // NAK means data has not changed since last call. Return true without
  // touching the state.
  if (result == hrNAK) {
    return true;
  }

  // If there's some other error, return false, since there's no joystick
  // indication.
  if (result) {
    return false;
  }

#if 0
  char s[100], *end = s;
  for (int i = 0; i < result_len; i++) {
    end += sprintf(end, " %02x", buf[i]);
  }
  LOG("got %d bytes:%s", result_len, s);
#endif

  if (result_len != 6) {
    LOG("Warning: thrustmaster got weird return length %d", result_len);
    return false;
  }

  joystate->x_pos = scale_to_99(buf[0]);
  joystate->y_pos = -scale_to_99(buf[1]);
  if (buf[4] & 0xf0) {
    joystate->state |= JOYSTICK_STATE_TRIGGER;
  } else {
    joystate->state &= ~(JOYSTICK_STATE_TRIGGER);
  }

  return true;
}
