#include <inttypes.h>

#include "display_controller.h"
#include "display_compass.h"
#include "util.h"
#include "random.h"
#include "focus.h"

void dcompass_update_once(DCompassAct *act);
void dcompass_update(DCompassAct *act);
UIEventDisposition dcompass_event_handler(
	UIEventHandler *raw_handler, UIEvent evt);

void dcompass_init(DCompassAct *act, uint8_t board, FocusAct *focus)
{
	act->func = (ActivationFunc) dcompass_update;
	board_buffer_init(&act->bbuf);
	board_buffer_push(&act->bbuf, board);
	act->offset = 0;
	act->handler.func = (UIEventHandlerFunc) dcompass_event_handler;
	act->handler.act = act;
	act->focused = FALSE;
	DisplayRect rect = {board, board, 0, 7};
	focus_register(focus, (UIEventHandler*) &act->handler, rect);
	schedule(0, (Activation*) act);
}

static char *compass_display = "N.,.3.,.S.,.E.,.N.,.3.,.S.,.E.,.";

void dcompass_update(DCompassAct *act)
{
	dcompass_update_once(act);
	uint32_t interval;
	if (act->focused)
	{
		interval = 128;
	}
	else
	{
		interval = 300+(deadbeef_rand() & 0x0ff);
	}
	schedule(interval, (Activation*) act);
}

void dcompass_update_once(DCompassAct *act)
{
	if (!act->focused)
	{
		int8_t shift = ((int8_t)(deadbeef_rand() % 3)) - 1;
		act->offset = (act->offset+shift) & 0x0f;
	}
	ascii_to_bitmap_str(act->bbuf.buffer, compass_display + act->offset);
	if (act->focused && (clock_time()>>7)&1)
	{
		act->bbuf.buffer[7] = 0b1001110;
		act->bbuf.buffer[0] = 0b1111000;
	}
	board_buffer_draw(&act->bbuf);
}

UIEventDisposition dcompass_event_handler(
	UIEventHandler *raw_handler, UIEvent evt)
{
	DCompassAct *act = ((DCompassHandler *) raw_handler)->act;
	UIEventDisposition result = uied_ignore;
	switch (evt)
	{
		case uie_focus:
			act->focused = TRUE;
			break;
		case uie_blur:
			act->focused = FALSE;
			break;
		case 'a':
			act->offset = (act->offset-1) & 0x0f;
			break;
		case 'b':
			act->offset = (act->offset+1) & 0x0f;
			break;
	}
	dcompass_update_once(act);
	return result;
}
