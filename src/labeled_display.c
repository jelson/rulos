#include "labeled_display.h"

UIEventDisposition labeled_display_event_handler(
	UIEventHandler *raw_handler, UIEvent evt);

void labeled_display_init(LabeledDisplayHandler *ldh, int b0, FocusAct *focus)
{
	ldh->func = labeled_display_event_handler;

	dscrlmsg_init(&ldh->msgAct, 0, " clock  ", b0);
	drtc_init(&ldh->rtcAct, b0+1);

	DisplayRect rect = {b0, b0+1, 1, 6};
	focus_register(focus, (UIEventHandler*) ldh, rect);
}

UIEventDisposition labeled_display_event_handler(
	UIEventHandler *raw_handler, UIEvent evt)
{
	// do nothing for now.
	return uied_ignore;
}

