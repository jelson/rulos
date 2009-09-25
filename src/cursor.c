#include <stdio.h>
#include "util.h"
#include "cursor.h"

#define BLINK2	(8)	// blink every 2^BLINK2 ms

void cursor_update_once(CursorAct *act);
void cursor_update(CursorAct *act);

void cursor_init(CursorAct *act)
{
	act->func = (ActivationFunc) cursor_update;
	int i;
	for (i=0; i<MAX_HEIGHT; i++)
	{
		board_buffer_init(&act->bbuf[i]);
	}
	act->visible = FALSE;
	act->shape_blank = FALSE;
	schedule(0, (Activation*) act);
}

void cursor_hide(CursorAct *act)
{
	if (act->visible)
	{
		int i;
		for (i=act->rect.y0; i<=act->rect.y1; i++)
		{
			board_buffer_pop(&act->bbuf[i-act->rect.y0]);
		}
		act->visible = FALSE;
	}
}

void cursor_show(CursorAct *act, DisplayRect rect)
{
	if (act->visible)
	{
		cursor_hide(act);
	}
	act->rect = rect;
	assert(rect.y1-rect.y0+1 <= MAX_HEIGHT);

	int i;
	for (i=rect.y0; i<=rect.y1; i++)
	{
		int bigmask =    (((int)1)<<(NUM_DIGITS-rect.x0))-1;
		int littlemask = ((((int)1)<<(NUM_DIGITS-rect.x1))-1)>>1;
		act->alpha = bigmask ^ littlemask;
		board_buffer_set_alpha(&act->bbuf[i-rect.y0], act->alpha);
		//LOGF((logfp, "x0 %d x1 %d alpha %08x\n", rect.x0, rect.x1, act->alpha));
		// TODO set up board buffer first, to avoid writing stale bytes
		board_buffer_push(&act->bbuf[i-rect.y0], i);
	}
	act->visible = TRUE;
	cursor_update_once(act);
}

void cursor_update_once(CursorAct *act)
{
	// lazy programmer leaves act scheduled all the time, even when
	// not visible.
	if (act->visible)
	{
		int i,j;
		for (i=act->rect.y0; i<=act->rect.y1; i++)
		{
			BoardBuffer *bbp = &act->bbuf[i-act->rect.y0];
			for (j=0; j<NUM_DIGITS; j++)
			{
				if (act->shape_blank)
				{
					bbp->buffer[j] = 0;
				}
				else
				{
					bbp->buffer[j] =
						  ((j==act->rect.x0)?0b0000110:0)
						| ((j==act->rect.x1)?0b0110000:0)
						| ((i==act->rect.y0)?0b1000000:0)
						| ((i==act->rect.y1)?0b0001000:0);
				}
			}
			uint8_t on = ((clock_time() >> BLINK2)&1);
			board_buffer_set_alpha(bbp, on ? act->alpha : 0);
			board_buffer_draw(bbp);
		}

	}
}

void cursor_update(CursorAct *act)
{
	cursor_update_once(act);
	schedule(1<<BLINK2, (Activation*) act);
}

void cursor_set_shape_blank(CursorAct *act, uint8_t value)
{
	act->shape_blank = value;
}
