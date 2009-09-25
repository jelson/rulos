#include "labeled_display.h"

UIEventDisposition labeled_display_event_handler(
	UIEventHandler *raw_handler, UIEvent evt);

void labeled_display_init(LabeledDisplayHandler *ldh, int b0, FocusManager *focus)
{
	ldh->func = labeled_display_event_handler;

	dscrlmsg_init(&ldh->msgAct, 0, " clock  ", b0);
	drtc_init(&ldh->rtcAct, b0+1);

	ldh->bufs[0] = &ldh->msgAct.bbuf;
	ldh->bufs[1] = &ldh->rtcAct.bbuf;
	RectRegion rr = {ldh->bufs, 2, 1, 6};
	focus_register(focus, (UIEventHandler*) ldh, rr);
}

UIEventDisposition labeled_display_event_handler(
	UIEventHandler *raw_handler, UIEvent evt)
{
	// most-trivial handler (note no cursor; sorry.)
	switch (evt)
	{
		case uie_escape:
			return uied_blur;
		default:
			return uied_accepted;
	}
}

