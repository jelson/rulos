#include <stdio.h>
#include <string.h>

#include "display_controller.h"
#include "display_scroll_msg.h"
#include "util.h"

void dscrlmsg_update(struct s_dscrollmsgact *act);

void dscrlmsg_init(struct s_dscrollmsgact *act,
	uint8_t board, char *msg, uint8_t speed_ms)
{
	act->func = (ActivationFunc) dscrlmsg_update;
	board_buffer_init(&act->bbuf);
	board_buffer_push(&act->bbuf, board);
	act->msg = msg;
	act->len = strlen(act->msg);
	act->speed_ms = speed_ms;
	act->index = 0;
	schedule(0, (Activation*) act);
}

void dscrlmsg_update(struct s_dscrollmsgact *act)
{
	if (act->speed_ms > 0)
	{
		schedule(act->speed_ms, (Activation*) act);
		act->index++;
	}
	uint8_t i;
	for (i=0; i<NUM_DIGITS; i++)
	{
		act->bbuf.buffer[i] = ascii_to_bitmap(act->msg[(act->index+i)%act->len]);
	}
	board_buffer_draw(&act->bbuf);
}
