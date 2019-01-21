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

#pragma once

#include "periph/display_rtc/display_rtc.h"
#include "periph/input_controller/focus.h"
#include "periph/rocket/display_scroll_msg.h"

typedef struct {
  UIEventHandlerFunc func;
  DScrollMsgAct msgAct;
  DRTCAct rtcAct;
  BoardBuffer *bufs[2];
} LabeledDisplayHandler;

void labeled_display_init(LabeledDisplayHandler *ldh, int b0,
                          FocusManager *focus);
