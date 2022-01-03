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

#include "core/rulos.h"
#include "periph/joystick/joystick.h"
#include "periph/max3421e/periph_thrustmaster.h"

max3421e_t max;
JoystickState_t joystate;

void poll_joystick(void *data) {
  schedule_us(100000, poll_joystick, NULL);

  if (thrustmaster_read(&max, &joystate)) {
    LOG("x=%d, y=%d, button=%s", joystate.x_pos, joystate.y_pos,
        joystate.state & JOYSTICK_STATE_TRIGGER ? "DOWN" : "UP");
  } else {
    LOG("No joy!");
  }
}

int main() {
  rulos_hal_init();

#ifdef LOG_TO_SERIAL
  UartState_t uart;
  uart_init(&uart, /* uart_id= */ 0, 115200);
  log_bind_uart(&uart);
#endif

  init_clock(10000, TIMER1);

  max3421e_init(&max);
  schedule_us(1, poll_joystick, NULL);

  cpumon_main_loop();
}
