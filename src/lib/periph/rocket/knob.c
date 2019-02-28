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

#include "periph/rocket/knob.h"
#include "periph/rocket/rocket.h"

UIEventDisposition knob_handler(UIEventHandler *raw_handler, UIEvent evt);
void knob_update_once(Knob *knob);

void knob_init(Knob *knob, RowRegion region, const char **msgs, uint8_t len,
               UIEventHandler *notify, FocusManager *fa, const char *label) {
  knob->func = knob_handler;
  knob->msgs = msgs;
  knob->len = len;
  knob->selected = 0;
  knob->region = region;
  cursor_init(&knob->cursor);
  cursor_set_label(&knob->cursor, cursor_label_white);
  knob->notify = notify;
  RectRegion rr = {&knob->region.bbuf, 1, knob->region.x, knob->region.xlen};
  focus_register(fa, (UIEventHandler *)knob, rr, label);
  knob_update_once(knob);
}

UIEventDisposition knob_handler(UIEventHandler *raw_handler, UIEvent evt) {
  UIEventDisposition result = uied_accepted;
  Knob *knob = (Knob *)raw_handler;
  switch (evt) {
    case uie_focus: {
      RectRegion rr = {&knob->region.bbuf, 1, knob->region.x,
                       knob->region.xlen};
      cursor_show(&knob->cursor, rr);
    } break;
    case uie_escape:
    case uie_select:
      cursor_hide(&knob->cursor);
      result = uied_blur;
      knob->notify->func(knob->notify, evt_notify);
      break;
    case uie_right:
      knob->selected = (knob->selected + 1) % knob->len;
      knob_update_once(knob);
      break;
    case uie_left:
      knob->selected = (knob->selected - 1 + knob->len) % knob->len;
      knob_update_once(knob);
      break;
  }
  return result;
}

void knob_update_once(Knob *knob) {
  ascii_to_bitmap_str(knob->region.bbuf->buffer + knob->region.x,
                      knob->region.xlen, knob->msgs[knob->selected]);
  board_buffer_draw(knob->region.bbuf);
}

void knob_set_value(Knob *knob, uint8_t value) {
  knob->selected = value;
  knob_update_once(knob);
}
