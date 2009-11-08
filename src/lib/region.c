#include "rocket.h"
#include "region.h"

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
