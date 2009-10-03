#include <stdio.h>
#include <string.h>
#include "display_blinker.h"

void blinker_update(DBlinker *blinker);
void blinker_update_once(DBlinker *blinker);

void blinker_init(DBlinker *blinker, uint8_t period2)
{
	blinker->func = (ActivationFunc) blinker_update;
	blinker->periodBit = ((uint16_t) 1) << period2;
	blinker->msg = NULL;
	blinker->cur_line = 0;
	board_buffer_init(&blinker->bbuf);
	schedule(1, (Activation*) blinker);
}

void blinker_set_msg(DBlinker *blinker, const char **msg)
{
	blinker->msg = msg;
	blinker->cur_line = 0;
	blinker_update_once(blinker);
}

void blinker_update_once(DBlinker *blinker)
{
	memset(&blinker->bbuf.buffer, 0, NUM_DIGITS);
	if (blinker->msg!=NULL)
	{
		ascii_to_bitmap_str(blinker->bbuf.buffer,
			NUM_DIGITS,
			blinker->msg[blinker->cur_line]);
	}
	board_buffer_draw(&blinker->bbuf);
}

void blinker_update(DBlinker *blinker)
{
	schedule(clock_time()+blinker->periodBit, (Activation*) blinker);
	blinker_update_once(blinker);
}
