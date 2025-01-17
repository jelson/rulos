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

#include "chip/avr/periph/usi_twi_slave/usi_twi_slave.h"
#include "core/clock.h"
#include "core/hal.h"
#include "core/hardware.h"
#include "core/rulos.h"
#include "core/util.h"

#define TEST_SLAVE_SEND_ONLY

#ifdef TEST_SLAVE_SEND_ONLY
// for slave-send-only test
uint8_t counter = 'a';

static uint8_t return_next_char() {
  if (counter == 'z' + 1)
    counter = 'a';

  return counter++;
}

#else

// for send-receive test
char inbuf[128];
char outbuf[128];
uint8_t outbuf_len = 0;
uint8_t outbuf_num_sent = 0;

static void receive_done(MediaRecvSlot *recv_slot, uint8_t len) {
  outbuf_len = len;
  for (int i = 0; i < len; i++) {
    //		outbuf[i] = recv_slot->data[i];
    gpio_set(GPIO_A5);
    gpio_clr(GPIO_A5);
  }
  outbuf_num_sent = 0;
}

static uint8_t return_recv_slot() {
  if (outbuf_len > outbuf_num_sent) {
    return outbuf[outbuf_num_sent++];
  } else {
    return 0xab;
  }
}
#endif

int main() {
  rulos_hal_init();

  // start clock with 10 msec resolution
  init_clock(10000, TIMER1);

#ifdef TEST_SLAVE_SEND_ONLY
  // start the USI module, with a send func that returns an increasing
  // sequence of characters
  usi_twi_slave_init(50 /* address */, NULL, return_next_char);
#else
  MediaRecvSlot *recv_slot = (MediaRecvSlot *)inbuf;
  recv_slot->capacity = sizeof(inbuf) - sizeof(MediaRecvSlot);
  recv_slot->func = receive_done;

  // start the USI module, with a send func that returns a capitalized
  // version of the most recently received data
  usi_twi_slave_init(50 /* address */, recv_slot, return_recv_slot);
#endif
  scheduler_run();
  assert(FALSE);
}
