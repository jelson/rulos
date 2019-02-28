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

#include "periph/rocket/labeled_display.h"
#include "core/util.h"
#include "periph/rocket/rocket.h"

UIEventDisposition labeled_display_event_handler(UIEventHandler *raw_handler,
                                                 UIEvent evt);

void labeled_display_init(LabeledDisplayHandler *ldh, int b0,
                          FocusManager *focus) {
  ldh->func = labeled_display_event_handler;

  dscrlmsg_init(&ldh->msgAct, 0, " clock  ", b0);
  drtc_init(&ldh->rtcAct, b0 + 1, 0);

  ldh->bufs[0] = &ldh->msgAct.bbuf;
  ldh->bufs[1] = &ldh->rtcAct.bbuf;
  RectRegion rr = {ldh->bufs, 2, 1, 6};
  focus_register(focus, (UIEventHandler *)ldh, rr, "clock");
}

UIEventDisposition labeled_display_event_handler(UIEventHandler *raw_handler,
                                                 UIEvent evt) {
  // most-trivial handler (note no cursor; sorry.)
  switch (evt) {
    case uie_escape:
      return uied_blur;
    default:
      return uied_accepted;
  }
}
