#include <stdio.h>
#include <string.h>

#include "display_gratuitous_graph.h"
#include "util.h"

void dgg_update(DGratuitousGraph *act);

void dgg_init(DGratuitousGraph *dgg,
	uint8_t board, char *name, uint16_t impulse_frequency)
{
	dgg->func = (ActivationFunc) dgg_update;
	board_buffer_init(&dgg->bbuf);
	board_buffer_push(&dgg->bbuf, board);
	drift_anim_init(&dgg->drift, 10, 0, 0, 16, 2);
	dgg->name = name;
	dgg->impulse_frequency = impulse_frequency;
	dgg->last_impulse = 0;
	schedule(0, (Activation*) dgg);
}

void dgg_update(DGratuitousGraph *dgg)
{
	schedule(50, (Activation*) dgg);
	uint32_t t = clock_time();
	if (t - dgg->last_impulse > dgg->impulse_frequency)
	{
		dgg->last_impulse = t;
		da_random_impulse(&dgg->drift);
	}
	if (((t >> 7) & 7) == 0)
	{
		// display label
		char str[8];
		int_to_string(str, 4, FALSE, da_read(&dgg->drift));
		LOGF((logfp, "dgg int str '%s'\n", str));
		ascii_to_bitmap_str(dgg->bbuf.buffer, 4, str);
		ascii_to_bitmap_str(dgg->bbuf.buffer, NUM_DIGITS, dgg->name);
	}
	else
	{
		// display bar graph
		int v = da_read(&dgg->drift);
		int d;
		for (d=0; d<NUM_DIGITS; d++)
		{
			uint8_t b = 0;
			if (d*2+1<v) b |= 0b0110000;
			if (d*2  <v) b |= 0b0000110;
			dgg->bbuf.buffer[d] = b;
		}
	}
	board_buffer_draw(&dgg->bbuf);
}
