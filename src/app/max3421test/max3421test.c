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

#include "core/rulos.h"
#include "periph/max3421e/max3421e.h"
#include "periph/max3421e/usbstructs.h"

#ifdef LOG_TO_SERIAL
HalUart uart;
#endif

max3421e_t max;

void poll_joystick(void *data) {
  schedule_us(10000, poll_joystick, NULL);

  usb_device_t *dev = &max.devices[0];

  if (!dev->ready) {
    return;
  }

  usb_endpoint_t *endpoint = &dev->endpoints[1];
  uint8_t buf[10];
  uint16_t result_len;
  uint8_t result =
      max3421e_read_data(dev, endpoint, buf, sizeof(buf), &result_len);

  if (result == hrNAK) {
    return;
  }

  if (result) {
    LOG("failed: %d", result);
  } else if (result_len == 0) {
    LOG("no data");
  } else {
    uint8_t button_down = (buf[4] & 0xf0);
    int8_t x = buf[0];
    int8_t y = buf[1];
    LOG("x=%d, y=%d, button=%s", x, y, button_down ? "DOWN" : "UP");
#if 1
    char s[100], *end = s;
    for (int i = 0; i < result_len; i++) {
      end += sprintf(end, " %02x", buf[i]);
    }
    LOG("got %d bytes:%s", result_len, s);
#endif
  }
}

int main() {
  hal_init();

#ifdef LOG_TO_SERIAL
  hal_uart_init(&uart, 115200, true, /* uart_id= */ 0);
  LOG("Log output running");
#endif

  init_clock(10000, TIMER1);

  max3421e_init(&max);
  schedule_us(1, poll_joystick, NULL);

  cpumon_main_loop();
}
