#include "rocket.h"
#include "display_compass.h"
#include "focus.h"

void dcompass_update_once(DCompassAct *act);
void dcompass_update(DCompassAct *act);
UIEventDisposition dcompass_event_handler(
	UIEventHandler *raw_handler, UIEvent evt);

void dcompass_init(DCompassAct *act, uint8_t board, FocusManager *focus)
{
	act->func = (ActivationFunc) dcompass_update;
	board_buffer_init(&act->bbuf);
	board_buffer_push(&act->bbuf, board);
	int32_t range = ((int32_t)1)<<24;
	drift_anim_init(&act->drift, 10, 0, -range, range, 3);
	act->last_impulse_time = 0;
	act->handler.func = (UIEventHandlerFunc) dcompass_event_handler;
	act->handler.act = act;
	act->focused = FALSE;
	act->btable = &act->bbuf;
	RectRegion rr = { &act->btable, 1, 0, 7};
	focus_register(focus, (UIEventHandler*) &act->handler, rr, "compass");
	schedule_us(1, (Activation*) act);
}

static char *compass_display = "N.,.3.,.S.,.E.,.N.,.3.,.S.,.E.,.";

void dcompass_update(DCompassAct *act)
{
	dcompass_update_once(act);
	uint32_t interval_us;
	if (act->focused)
	{
		interval_us = 128000;
	}
	else
	{
		interval_us = (300+(deadbeef_rand() & 0x0ff)) * 1000;
	}
	schedule_us(interval_us, (Activation*) act);
}

void dcompass_update_once(DCompassAct *act)
{
	if (!act->focused)
	{
		Time t = clock_time_us();
		if (t - act->last_impulse_time > 3000000)
		{
			da_random_impulse(&act->drift);
			// drift is bounded, but compass rolls indefinitely.
			// snap back near zero to avoid "caging" compass at max/min
			da_set_value(&act->drift, da_read(&act->drift) & 0x0f);
			act->last_impulse_time = t;
		}
	}
	ascii_to_bitmap_str(act->bbuf.buffer,
		NUM_DIGITS,
		compass_display + (da_read(&act->drift) & 0x0f));
	if (act->focused && (clock_time_us()>>17)&1)
	{
		act->bbuf.buffer[7] = 0b1111000;
		act->bbuf.buffer[0] = 0b1001110;
	}
	board_buffer_draw(&act->bbuf);
}

UIEventDisposition dcompass_event_handler(
	UIEventHandler *raw_handler, UIEvent evt)
{
	DCompassAct *act = ((DCompassHandler *) raw_handler)->act;
	UIEventDisposition result = uied_accepted;
	switch (evt)
	{
		case uie_focus:
			act->focused = TRUE;
			da_set_velocity(&act->drift, 0);
			break;
		case uie_escape:
			act->focused = FALSE;
			result = uied_blur;
			break;
		case uie_right:
			da_set_value(&act->drift, da_read(&act->drift)-1);
			break;
		case uie_left:
			da_set_value(&act->drift, da_read(&act->drift)+1);
			break;
	}
	dcompass_update_once(act);
	return result;
}