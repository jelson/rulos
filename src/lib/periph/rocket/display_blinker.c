/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
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

#include "periph/rocket/display_blinker.h"
#include "core/util.h"
#include "periph/rocket/rocket.h"

void blinker_update(DBlinker *blinker);
void blinker_update_once(DBlinker *blinker);

void blinker_init(DBlinker *blinker, uint16_t period) {
  blinker->period = period;
  blinker->msg = NULL;
  blinker->cur_line = 0;
  board_buffer_init(&blinker->bbuf DBG_BBUF_LABEL("blinker"));
  schedule_us(1, (ActivationFuncPtr)blinker_update, blinker);
}

void blinker_set_msg(DBlinker *blinker, const char **msg) {
  blinker->msg = msg;
  blinker->cur_line = 0;
  blinker_update_once(blinker);
}

void blinker_update_once(DBlinker *blinker) {
  // LOG("blinker->cur_line = %d", blinker->cur_line);
  memset(&blinker->bbuf.buffer, 0, NUM_DIGITS);
  if (blinker->msg != NULL && blinker->msg[blinker->cur_line] != NULL) {
    ascii_to_bitmap_str(blinker->bbuf.buffer, NUM_DIGITS,
                        blinker->msg[blinker->cur_line]);
  }
  board_buffer_draw(&blinker->bbuf);
}

void blinker_update(DBlinker *blinker) {
  schedule_us(blinker->period * 1000, (ActivationFuncPtr)blinker_update,
              blinker);
  blinker_update_once(blinker);

  blinker->cur_line += 1;
  if (blinker->msg == NULL || blinker->msg[blinker->cur_line] == NULL) {
    blinker->cur_line = 0;
  }
}
