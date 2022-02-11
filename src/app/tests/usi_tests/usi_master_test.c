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

#include <ctype.h>
#include <inttypes.h>

#include "chip/avr/periph/usi_twi_master/usi_twi_master.h"
#include "core/clock.h"
#include "core/hal.h"
#include "core/hardware.h"
#include "core/rulos.h"
#include "core/util.h"

int i = 0;

void send_init_message() {
  char buf[17];

  buf[0] = 0x10;

  for (int i = 1; i <= 16; i++) {
    buf[i] = 0xff;
  }

  usi_twi_master_send(0b1110100, buf, 17);

  buf[0] = 0xB0;
  buf[1] = 0;

  usi_twi_master_send(0b1110100, buf, 2);
}

void send_message(void* data) {
  send_init_message();

  char buf[30];
  // sprintf(buf, "hello this is test %d\n", i++);
  // usi_twi_master_send(4, buf, strlen(buf));

  i++;
  buf[0] = 0;  // start address is address 0
  buf[1] = 0;  // address 0: data 0

  if (i % 2 == 0) {
    buf[2] = 0xff;  // addr 1: data ff
    buf[3] = 0;     // addr 2: data 0
  } else {
    buf[2] = 0;     // addr 1: data 0
    buf[3] = 0xff;  // addr 2: data ff
  }

  usi_twi_master_send(0b1110100, buf, 4);
  schedule_us(1000000, send_message, NULL);
}

int main() {
  rulos_hal_init();

  // start clock with 10 msec resolution
  init_clock(10000, TIMER1);

  usi_twi_master_init();
  schedule_now(send_message, NULL);

  scheduler_run();
  assert(FALSE);
}
