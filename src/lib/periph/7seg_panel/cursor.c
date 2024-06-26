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

#include "periph/7seg_panel/cursor.h"

#include <stdlib.h>

#include "core/clock.h"
#include "core/rulos.h"

#define BLINK2 (18)  // blink every 256ms

char cursor_label_white[1];
void cursor_update_once(CursorAct *act);
void cursor_update(void *data);

void cursor_init(CursorAct *act) {
  int i;
  for (i = 0; i < MAX_HEIGHT; i++) {
    board_buffer_init(&act->bbuf[i] DBG_BBUF_LABEL("cursor"));
  }
  act->visible = FALSE;
  act->label = NULL;
  schedule_us(1, cursor_update, act);
}

void cursor_hide(CursorAct *act) {
  if (act->visible) {
    int i;
    for (i = 0; i < act->rr.ylen; i++) {
      board_buffer_pop(&act->bbuf[i]);
    }
    act->visible = FALSE;
  }
}

void cursor_show(CursorAct *act, RectRegion rr) {
  if (act->visible) {
    cursor_hide(act);
  }
  act->rr = rr;
  assert(rr.ylen <= MAX_HEIGHT);

  int i;
  for (i = 0; i < rr.ylen; i++) {
    int bigmask = (((int)1) << (NUM_DIGITS - rr.x)) - 1;
    uint8_t x1 = rr.x + rr.xlen - 1;
    int littlemask = ((((int)1) << (NUM_DIGITS - x1)) - 1) >> 1;
    act->alpha = bigmask ^ littlemask;
    board_buffer_set_alpha(&act->bbuf[i], act->alpha);
    // LOG("x0 %d x1 %d alpha %08x", rect.x0, rect.x1, act->alpha);
    // TODO set up board buffer first, to avoid writing stale bytes
    board_buffer_push(&act->bbuf[i], rr.bbuf[i]->board_index);
  }
  act->visible = TRUE;
  cursor_update_once(act);
}

void cursor_update_once(CursorAct *act) {
  // lazy programmer leaves act scheduled all the time, even when
  // not visible.
  if (act->visible) {
    int i, j;
    uint8_t x0 = act->rr.x;
    uint8_t x1 = act->rr.x + act->rr.xlen - 1;
    for (i = 0; i < act->rr.ylen; i++) {
      BoardBuffer *bbp = &act->bbuf[i];
      for (j = 0; j < NUM_DIGITS; j++) {
        if (act->label == cursor_label_white) {
          bbp->buffer[j] = 0;
        } else {
          bbp->buffer[j] = ((j == x0) ? 0b0000110 : 0) |
                           ((j == x1) ? 0b0110000 : 0) |
                           ((i == 0) ? 0b1000000 : 0) |
                           ((i == act->rr.ylen - 1) ? 0b0001000 : 0);
        }
      }

      // apply label to last line
      if (i == act->rr.ylen - 1 && act->label != cursor_label_white &&
          act->label != NULL) {
        ascii_to_bitmap_str(act->bbuf[act->rr.ylen - 1].buffer + act->rr.x + 1,
                            act->rr.xlen - 2, act->label);
      }

      // hide cursor half the time
      uint8_t on = ((clock_time_us() >> BLINK2) & 1);
      board_buffer_set_alpha(bbp, on ? act->alpha : 0);

      board_buffer_draw(bbp);
    }
  }
}

void cursor_update(void *data) {
  cursor_update_once((CursorAct *)data);
  schedule_us(Exp2Time(BLINK2), cursor_update, data);
}

void cursor_set_label(CursorAct *act, const char *label) {
  act->label = label;
  cursor_update_once(act);
}
