#include "screenblanker.h"

UIEventDisposition screenblanker_handler(ScreenBlanker *screenblanker, UIEvent evt);
void screenblanker_update(ScreenBlankerClockAct *act);
void screenblanker_update_once(ScreenBlanker *sb);

void init_screenblanker(ScreenBlanker *screenblanker, BoardConfiguration bc, HPAM *hpam, IdleAct *idle)
{
	assert(bc==bc_rocket0 || bc==bc_rocket1);
	screenblanker->func = (UIEventHandlerFunc) screenblanker_handler;
	screenblanker->num_buffers = bc==bc_rocket0 ? 8 : 4;

	int i;
	for (i=0; i<screenblanker->num_buffers; i++)
	{
		// hpam_max_alpha keeps us, while doodling with our alphas,
		// from ever overwriting the hpam's special segments.
		if (hpam!=NULL && i==hpam->bbuf.board_index)
		{
			screenblanker->hpam_max_alpha[i] = ~hpam->bbuf.alpha;
		}
		else
		{
			screenblanker->hpam_max_alpha[i] = 0xff;
		}

		board_buffer_init(&screenblanker->buffer[i] DBG_BBUF_LABEL("screenblanker"));
	}

	screenblanker->mode = sb_inactive;

	screenblanker->clock_act.func = (ActivationFunc) screenblanker_update;
	screenblanker->clock_act.sb = screenblanker;
	schedule_us(1, (Activation*) &screenblanker->clock_act);

	idle_add_handler(idle, (UIEventHandler *) screenblanker);
}

UIEventDisposition screenblanker_handler(ScreenBlanker *screenblanker, UIEvent evt)
{
	if (evt == evt_idle_nowidle)
	{
		screenblanker_setmode(screenblanker, sb_flicker);
	}
	else if (evt == evt_idle_nowactive)
	{
		// system is active, so screenblanker is inactive.
		screenblanker_setmode(screenblanker, sb_inactive);
	}
	return uied_accepted;
}

void screenblanker_setmode(ScreenBlanker *screenblanker, ScreenBlankerMode newmode)
{
	int i;

	if (newmode==screenblanker->mode)
	{
		return;
	}
	else if (newmode==sb_inactive)
	{
		for (i=0; i<screenblanker->num_buffers; i++)
		{
			if (board_buffer_is_stacked(&screenblanker->buffer[i]))
			{
				board_buffer_pop(&screenblanker->buffer[i]);
			}
		}
	}
	else
	{
		for (i=0; i<screenblanker->num_buffers; i++)
		{
			if (!board_buffer_is_stacked(&screenblanker->buffer[i]))
			{
				board_buffer_push(&screenblanker->buffer[i], i);
			}
		}
	}
	screenblanker->mode = newmode;
	screenblanker_update_once(screenblanker);
}

void screenblanker_update(ScreenBlankerClockAct *act)
{
	ScreenBlanker *sb = act->sb;
	Time interval = 100000;	// update 0.1s when not doing animation
	if (sb->mode==sb_flicker || sb->mode==sb_flicker)
	{
		interval =   40000;	// update 0.01s when animating
	}
	schedule_us(interval, (Activation*) act);

	screenblanker_update_once(sb);
}

void screenblanker_update_once(ScreenBlanker *sb)
{
	int i, j;
	switch (sb->mode)
	{
	case sb_inactive:
		break;
	case sb_blankdots:
	{
		for (i=0; i<sb->num_buffers; i++) {
			for (j=0; j<NUM_DIGITS; j++) {
				sb->buffer[i].buffer[j] = SSB_DECIMAL;
			}
			board_buffer_set_alpha(&sb->buffer[i], sb->hpam_max_alpha[i]);
		}
		break;
	}
	case sb_black:
	{
		for (i=0; i<sb->num_buffers; i++) {
			for (j=0; j<NUM_DIGITS; j++) {
				sb->buffer[i].buffer[j] = 0;
			}
			board_buffer_set_alpha(&sb->buffer[i], sb->hpam_max_alpha[i]);
		}
		break;
	}
	case sb_disco:
	{
		for (i=0; i<sb->num_buffers; i++) {
			for (j=0; j<NUM_DIGITS; j++) {
				sb->buffer[i].buffer[j] = 0;
			}
			board_buffer_set_alpha(&sb->buffer[i], sb->hpam_max_alpha[i]);
		}
		break;
	}
	case sb_flicker:
	{
		SSBitmap color = (clock_time_us() % 79) < 39 ? 0xff : 0;
		color = 0;
		for (i=0; i<sb->num_buffers; i++) {
			uint8_t alpha =
				//deadbeef_rand() &
				deadbeef_rand()
				& sb->hpam_max_alpha[i];
			for (j=0; j<NUM_DIGITS; j++) {
				sb->buffer[i].buffer[j] = color;
			}
			board_buffer_set_alpha(&sb->buffer[i], alpha);
			// NB 'set_alpha includes a redraw.
		}
		break;
	}
	}
}
