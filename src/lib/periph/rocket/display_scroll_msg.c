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

#include "periph/rocket/display_scroll_msg.h"

#include <stdio.h>
#include <string.h>

#include "periph/rocket/rocket.h"

void dscrlmsg_update(DScrollMsgAct *act);

void dscrlmsg_init(DScrollMsgAct *act, uint8_t board, const char *msg,
                   uint8_t speed_ms) {
  board_buffer_init(&act->bbuf DBG_BBUF_LABEL("scrlmsg"));
  board_buffer_push(&act->bbuf, board);
  act->len = 0;
  act->speed_ms = speed_ms;
  act->index = 0;
  dscrlmsg_set_msg(act, msg);
  schedule_us(1, (ActivationFuncPtr)dscrlmsg_update, act);
}

int dscrlmsg_nexti(DScrollMsgAct *act, int i) {
  i = i + 1;
  if (i >= act->len) {
    i = 0;
  }
  return i;
}

void dscrlmsg_update_once(DScrollMsgAct *act) {
  int si = act->index;
  uint8_t di;
  for (di = 0; di < NUM_DIGITS; di++) {
    int ni = dscrlmsg_nexti(act, si);
    act->bbuf.buffer[di] = ascii_to_bitmap(act->msg[si]);
    si = ni;
  }
  board_buffer_draw(&act->bbuf);
}

void dscrlmsg_update(DScrollMsgAct *act) {
  if (act->speed_ms > 0) {
    schedule_us(((Time)act->speed_ms) * 1000,
                (ActivationFuncPtr)dscrlmsg_update, act);
    if (act->len > NUM_DIGITS) {
      act->index = dscrlmsg_nexti(act, act->index);
    } else {
      act->index = 0;
    }
  }
  dscrlmsg_update_once(act);
}

void dscrlmsg_set_msg(DScrollMsgAct *act, const char *msg) {
  act->msg = msg;
  act->len = strlen(act->msg);
  if (act->index > act->len) {
    act->index = 0;
  }
  dscrlmsg_update_once(act);
}
