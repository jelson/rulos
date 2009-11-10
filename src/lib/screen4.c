#include "screen4.h"

#include "rocket.h"
#include "region.h"

void init_screen4(Screen4 *s4, uint8_t board0)
{
	s4->board0 = board0;
	int r;
	for (r=0; r<SCREEN4SIZE; r++)
	{
		board_buffer_init(&s4->bbuf[r]);
		s4->bbufp[r] = &s4->bbuf[r];
	}
	s4->rrect.bbuf = s4->bbufp;
	s4->rrect.ylen = 4;
	s4->rrect.x = 0;
	s4->rrect.xlen = 8;
}

void s4_show(Screen4 *s4)
{
	int r;
	for (r=0; r<SCREEN4SIZE; r++)
	{
		if (!board_buffer_is_stacked(&s4->bbuf[r]))
		{
			board_buffer_push(&s4->bbuf[r], s4->board0+r);
		}
	}
}

void s4_hide(Screen4 *s4)
{
	int r;
	for (r=0; r<SCREEN4SIZE; r++)
	{
		if (board_buffer_is_stacked(&s4->bbuf[r]))
		{
			board_buffer_pop(&s4->bbuf[r]);
		}
	}
}
