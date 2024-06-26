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

/////////////////////////////

#include "chip/avr/periph/usi_serial/usi_serial.h"
#include "core/rulos.h"

int main() {
  rulos_hal_init();

  while (true) {
    for (int i = 0; i < 10000; i++) {
      char buf[50];
      sprintf(buf, "This is test number %05d", i);
      usi_serial_send(buf);
    }
  }
}
