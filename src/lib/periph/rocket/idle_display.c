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

#include "periph/rocket/idle_display.h"

#include "periph/rocket/display_scroll_msg.h"
#include "periph/rocket/rocket.h"

void idle_display_update(IdleDisplayAct *act);
void idle_display_init(IdleDisplayAct *act, DScrollMsgAct *scrollAct) {
  act->scrollAct = scrollAct;
  schedule_us(1, (ActivationFuncPtr)idle_display_update, act);
}

void idle_display_update(IdleDisplayAct *act) {
  strcpy(act->msg, "busy ");
  int busy = 0; // TODO: reimplement cpu business metric
  int d = int_to_string2(act->msg + 5, 2, 0, busy);
  strcpy(act->msg + 5 + d, "%");
  dscrlmsg_set_msg(act->scrollAct, act->msg);
  schedule_us(1000000, (ActivationFuncPtr)idle_display_update, act);
}
