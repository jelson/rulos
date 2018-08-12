/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

#include "periph/rocket/screen4.h"

#include "periph/7seg_panel/region.h"
#include "periph/rocket/rocket.h"

void init_screen4(Screen4 *s4, uint8_t board0)
{
	s4->board0 = board0;
	int r;
	for (r=0; r<SCREEN4SIZE; r++)
	{
		board_buffer_init(&s4->bbuf[r] DBG_BBUF_LABEL("s4"));
		s4->bbufp[r] = &s4->bbuf[r];
	}
	s4->rrect.bbuf = s4->bbufp;
	s4->rrect.ylen = 4;
	s4->rrect.x = 0;
	s4->rrect.xlen = NUM_DIGITS;
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

void s4_draw(Screen4 *s4)
{
	int r;
	for (r=0; r<SCREEN4SIZE; r++)
	{
		board_buffer_draw(&s4->bbuf[r]);
	}
}

r_bool s4_visible(Screen4 *s4)
{
	return board_buffer_is_stacked(&s4->bbuf[0]);
}
