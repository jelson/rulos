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

void labeled_display_init(LabeledDisplayHandler *ldh, int b0, FocusAct *focus);
#endif // _labeled_display_h
