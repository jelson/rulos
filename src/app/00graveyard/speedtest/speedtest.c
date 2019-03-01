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

//#include <avr/boot.h>
#include <avr/io.h>
//#include <avr/interrupt.h>
//#include <util/delay_basic.h>
//#include <inttypes.h>
//#include <stdio.h>
//#include <string.h>

#include "core/hardware.h"
#include "periph/rocket/rocket.h"

int main() {
  uint8_t data_array[128];
  uint8_t next[128];
  uint16_t i;

  for (i = 0; i < sizeof(data_array); i++) {
    switch (i % 4) {
      case 0:
        data_array[i] = 0;
        break;
      case 1:
        data_array[i] = 0b01010101;
        break;
      case 2:
        data_array[i] = 0b11111111;
        break;
      case 3:
        data_array[i] = 0b10101010;
        break;
    }
    next[i] = i + 1;
  }
  next[63] = 0;

  DDRC = 0xff;
  i = 0;
  //	uint8_t *last = &data_array[64];
  //	uint8_t *ip = data_array;
  while (1) {
    PORTC = data_array[i];
    i = next[i];
  }
}
