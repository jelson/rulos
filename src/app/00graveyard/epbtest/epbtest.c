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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "periph/rocket/rocket.h"

#define DISPLAYBOARD 0
/****************************************************************************/

typedef struct {
  ActivationFunc func;
} TestAct;

void test_update(TestAct *act);

void test_init(TestAct *act) {
  act->func = (ActivationFunc)test_update;
  schedule_us(1, (Activation *)act);
}

void test_update(TestAct *act) {
  schedule_us(1, (Activation *)act);
  uint8_t digit, segment, board;
  for (board = 0; board < NUM_BOARDS; board++) {
    if (board == DISPLAYBOARD) continue;
    if (board != 4 && board != 5) continue;
    for (digit = 0; digit < NUM_DIGITS; digit++) {
      for (segment = 0; segment < 8; segment++) {
        uint8_t chosen_segment = (((clock_time_us() >> 18) + digit) & 7);
        uint8_t onoff = (chosen_segment == segment);
        //				if ((clock_time_us()>>21)&1)
        //					onoff = !onoff;
        hal_program_segment(board, digit, segment, onoff);
      }
    }
  }

  uint16_t delay = hal_read_adc(0);
#if !SIM
  extern uint16_t g_epb_delay_constant;
  g_epb_delay_constant = delay;
#endif
  delay += 1;
  char buf[9];
  memset(buf, '-', 8);
  int_to_string2(buf, 6, 0, delay);
  memset(buf + 6, '-', 2);
  SSBitmap bm[8];
  ascii_to_bitmap_str(bm, 8, buf);
  program_board(DISPLAYBOARD, bm);
}
/****************************************************************************/

int main() {
  util_init();
  rulos_hal_init(bc_rocket0);

#if 0
	init_clock(1000, TIMER1);
	hal_init_adc(10000);
	hal_init_adc_channel(0);

	TestAct test;
	test_init(&test);

	scheduler_run();
#else
#if !SIM
  extern void debug_abuse_epb();
  debug_abuse_epb();
#endif
#endif
  return 0;
}
