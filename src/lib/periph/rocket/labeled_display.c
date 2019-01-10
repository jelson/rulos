/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

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
