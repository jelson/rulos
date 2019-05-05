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

#include "periph/display_rtc/display_rtc.h"
#include "core/rulos.h"
#include "periph/input_controller/input_controller.h"

void drtc_update(DRTCAct *act);
void drtc_update_once(DRTCAct *act);

void drtc_init(DRTCAct *act, uint8_t board, Time base_time) {
  act->base_time = base_time;
  board_buffer_init(&act->bbuf DBG_BBUF_LABEL("rtc"));
  board_buffer_push(&act->bbuf, board);
  schedule_us(1, (ActivationFuncPtr)drtc_update, act);
}

void drtc_update(DRTCAct *act) {
  // Even though we only intend to update every 10ms, we schedule
  // every 9999, so that the clock update happens before
  // remote-board-buffer-draw, which also happens every 10ms. Even
  // though the rocket clock res is only 10000, the 9999 will be
  // pulled out of the heap first.
  schedule_us(9999, (ActivationFuncPtr)drtc_update, act);
  drtc_update_once(act);
}

void drtc_update_once(DRTCAct *act) {
  Time c = (clock_time_us() - act->base_time) / 1000;

  char buf[16], *p = buf;
  p += int_to_string2(p, 8, 3, c / 10);
  buf[0] = 't';

  ascii_to_bitmap_str(act->bbuf.buffer, NUM_DIGITS, buf);
  // jonh hard-codes decimal point
  act->bbuf.buffer[5] |= SSB_DECIMAL;
  board_buffer_draw(&act->bbuf);
}

void drtc_set_base_time(DRTCAct *act, Time base_time) {
  act->base_time = base_time;
  drtc_update_once(act);
}

Time drtc_get_base_time(DRTCAct *act) { return act->base_time; }
