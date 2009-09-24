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
	drift_anim_init(&act->drift, 10, 0, -(1<<20), 1<<20, 3);
	act->last_impulse_time = 0;
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
		uint32_t t = clock_time();
		if (t - act->last_impulse_time > 3000)
		{
			da_random_impulse(&act->drift);
			// drift is bounded, but compass rolls indefinitely.
			// snap back near zero to avoid "caging" compass at max/min
			da_set_value(&act->drift, da_read(&act->drift) & 0x0f);
			act->last_impulse_time = t;
		}
	}
	ascii_to_bitmap_str(act->bbuf.buffer,
		compass_display + (da_read(&act->drift) & 0x0f));
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
			da_set_velocity(&act->drift, 0);
			break;
		case uie_blur:
			act->focused = FALSE;
			break;
		case 'a':
			da_set_value(&act->drift, da_read(&act->drift)-1);
			break;
		case 'b':
			da_set_value(&act->drift, da_read(&act->drift)+1);
			break;
	}
	dcompass_update_once(act);
	return result;
}
