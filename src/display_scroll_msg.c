#include <stdio.h>
#include <string.h>

#include "rocket.h"
#include "display_scroll_msg.h"

void dscrlmsg_update(struct s_dscrollmsgact *act);

void dscrlmsg_init(struct s_dscrollmsgact *act,
	uint8_t board, char *msg, uint8_t speed_ms, uint8_t useHalf)
{
	act->func = (ActivationFunc) dscrlmsg_update;
	board_buffer_init(&act->bbuf);
	board_buffer_push(&act->bbuf, board);
	act->len = 0;
	act->speed_ms = speed_ms;
	act->index = 0;
	act->half = 0;
	act->useHalf = useHalf;
	dscrlmsg_set_msg(act, msg);
	schedule(1, (Activation*) act);
}

int dscrlmsg_nexti(DScrollMsgAct *act, int i)
{
	i = i + 1;
	if (i>=act->len)
	{
		i = 0;
	}
	return i;
}

void dscrlmsg_update_once(DScrollMsgAct *act)
{
	int si=act->index;
	uint8_t di;
	for (di=0; di<NUM_DIGITS; di++)
	{
		int ni = dscrlmsg_nexti(act, si);
		if (act->half)
		{
			SSBitmap leftChar = ascii_to_bitmap(act->msg[si]);
			SSBitmap rightChar = ascii_to_bitmap(act->msg[ni]);
			SSBitmap myChar = ((leftChar & 0b0100000) >> 4)
							| ((leftChar & 0b0010000) >> 2)
							| ((rightChar & 0b0000010) << 4)
							| ((rightChar & 0b0000100) << 2);
			act->bbuf.buffer[di] = myChar;
		}
		else
		{
			act->bbuf.buffer[di] = ascii_to_bitmap(act->msg[si]);
		}
		si = ni;
	}
	board_buffer_draw(&act->bbuf);
}

void dscrlmsg_update(DScrollMsgAct *act)
{
	if (act->speed_ms > 0)
	{
		int offset;
		if (act->useHalf)
		{
			if (act->half)
			{
				offset = (act->speed_ms*3) >> 2;
			}
			else
			{
				offset = (act->speed_ms*1) >> 2;
			}
		}
		else
		{
			offset = act->speed_ms;
		}
		schedule(offset, (Activation*) act);
		if (act->len > NUM_DIGITS)
		{
			if (act->half || !act->useHalf)
			{
				act->half = 0;
				act->index = dscrlmsg_nexti(act, act->index);
			}
			else
			{
				act->half = 1;
			}
		}
		else
		{
			act->index = 0;
			act->half = 0;
		}
	}
	dscrlmsg_update_once(act);
}

void dscrlmsg_set_msg(DScrollMsgAct *act, char *msg)
{
	act->msg = msg;
	act->len = strlen(act->msg);
	if (act->index > act->len)
	{
		act->index = 0;
		act->half = 0;
	}
	dscrlmsg_update_once(act);
}
