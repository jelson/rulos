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

#ifndef _labeled_display_h
#define _labeled_display_h

#include "display_rtc.h"
#include "display_scroll_msg.h"

#include "focus.h"

typedef struct {
	UIEventHandlerFunc func;
	DScrollMsgAct msgAct;
	DRTCAct rtcAct;
	BoardBuffer *bufs[2];
} LabeledDisplayHandler;

void labeled_display_init(LabeledDisplayHandler *ldh, int b0, FocusManager *focus);
#endif // _labeled_display_h
