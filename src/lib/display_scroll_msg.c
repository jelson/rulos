#include <stdio.h>
#include <string.h>

#include "rocket.h"
#include "display_scroll_msg.h"

void dscrlmsg_update(struct s_dscrollmsgact *act);

void dscrlmsg_init(struct s_dscrollmsgact *act,
	uint8_t board, char *msg, uint8_t speed_ms)
{
	act->func = (ActivationFunc) dscrlmsg_update;
	board_buffer_init(&act->bbuf DBG_BBUF_LABEL("scrlmsg"));
	board_buffer_push(&act->bbuf, board);
	act->len = 0;
	act->speed_ms = speed_ms;
	act->index = 0;
	dscrlmsg_set_msg(act, msg);
	schedule_us(1, (Activation*) act);
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
		act->bbuf.buffer[di] = ascii_to_bitmap(act->msg[si]);
		si = ni;
	}
	board_buffer_draw(&act->bbuf);
}

void dscrlmsg_update(DScrollMsgAct *act)
{
	if (act->speed_ms > 0)
	{
		schedule_us(((Time) act->speed_ms)*1000, (Activation*) act);
		if (act->len > NUM_DIGITS)
		{
			act->index = dscrlmsg_nexti(act, act->index);
		}
		else
		{
			act->index = 0;
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
	}
	dscrlmsg_update_once(act);
}
