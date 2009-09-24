#include <stdio.h>
#include "util.h"
#include "cursor.h"

#define BLINK2	(8)	// blink every 2^BLINK2 ms

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
	schedule(0, (Activation*) act);
}

void cursor_hide(CursorAct *act)
{
	if (act->visible)
	{
		int i;
		for (i=act->y0; i<=act->y1; i++)
		{
			board_buffer_pop(&act->bbuf[i-act->y0]);
		}
		act->visible = FALSE;
	}
}

void cursor_show(CursorAct *act,
	uint8_t y0, uint8_t y1, uint8_t x0, uint8_t x1)
{
	if (act->visible)
	{
		cursor_hide(act);
	}
	act->y0 = y0;
	act->y1 = y1;
	act->x0 = x0;
	act->x1 = x1;
	assert(y1-y0+1 <= MAX_HEIGHT);

	int i;
	for (i=y0; i<=y1; i++)
	{
		act->bbuf[i-y0].alpha = ((1<<(x0-1))-1) ^ ((1<<x1)-1);
		board_buffer_push(&act->bbuf[i-y0], i);
	}
	act->visible = TRUE;
}

void cursor_update(CursorAct *act)
{
	// lazy programmer leaves act scheduled all the time, even when
	// not visible.
	if (act->visible)
	{
		int i,j;
		for (i=act->y0; i<=act->y1; i++)
		{
			for (j=act->x0; j<=act->x1; j++)
			{
				if ((clock_time() >> BLINK2)&1)
				{
					act->bbuf[i-act->y0].buffer[j] =
						  ((j==act->x0)?0b0110000:0)
						| ((j==act->x1-1)?0b0000110:0)
						| ((i==act->y0)?0b1000000:0)
						| ((i==act->y1)?0b0001000:0);
				} else {
					act->bbuf[i-act->y0].buffer[j] = 0;
				}
			}
			board_buffer_draw(&act->bbuf[i-act->y0]);
		}

	}
	schedule(1<<BLINK2, (Activation*) act);
}
