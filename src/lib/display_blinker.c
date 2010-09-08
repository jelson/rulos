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

#include "rocket.h"
#include "display_blinker.h"
#include "util.h"

void blinker_update(DBlinker *blinker);
void blinker_update_once(DBlinker *blinker);

void blinker_init(DBlinker *blinker, uint16_t period)
{
	blinker->func = (ActivationFunc) blinker_update;
	blinker->period = period;
	blinker->msg = NULL;
	blinker->cur_line = 0;
	board_buffer_init(&blinker->bbuf DBG_BBUF_LABEL("blinker"));
	schedule_us(1, (Activation*) blinker);
}

void blinker_set_msg(DBlinker *blinker, const char **msg)
{
	blinker->msg = msg;
	blinker->cur_line = 0;
	blinker_update_once(blinker);
}

void blinker_update_once(DBlinker *blinker)
{
	//LOGF((logfp, "blinker->cur_line = %d\n", blinker->cur_line));
	memset(&blinker->bbuf.buffer, 0, NUM_DIGITS);
	if (blinker->msg!=NULL && blinker->msg[blinker->cur_line]!=NULL)
	{
		ascii_to_bitmap_str(blinker->bbuf.buffer,
			NUM_DIGITS,
			blinker->msg[blinker->cur_line]);
	}
	board_buffer_draw(&blinker->bbuf);
}

void blinker_update(DBlinker *blinker)
{
	schedule_us(blinker->period*1000, (Activation*) blinker);
	blinker_update_once(blinker);

	blinker->cur_line += 1;
	if (blinker->msg==NULL || blinker->msg[blinker->cur_line] == NULL)
	{
		blinker->cur_line = 0;
	}
}
