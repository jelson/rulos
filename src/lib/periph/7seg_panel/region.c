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

#include "periph/7seg_panel/region.h"

void region_hide(RectRegion *rr)
{
	int i;
	for (i=0; i<rr->ylen; i++)
	{
		board_buffer_pop(rr->bbuf[i]);
	}
}

void region_show(RectRegion *rr, int board0)
{
	int i;
	for (i=0; i<rr->ylen; i++)
	{
		board_buffer_push(rr->bbuf[i], board0+i);
	}
}
