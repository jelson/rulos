#include <stdio.h>
#include <string.h>

#include "display_controller.h"
#include "display_scroll_msg.h"
#include "util.h"

void dscrlmsg_update(struct s_dscrollmsgact *act);

void dscrlmsg_init(struct s_dscrollmsgact *act,
	uint8_t board, char *msg, uint8_t speed_ms)
{
	act->activation = (ActivationFunc) dscrlmsg_update;
	act->board = board;
	act->msg = msg;
	act->len = strlen(act->msg);
	act->speed_ms = speed_ms;
	schedule(0, (Activation*) act);
}

void dscrlmsg_update(struct s_dscrollmsgact *act)
{
	schedule(act->speed_ms, (Activation*) act);
	int index = (clock_time() / act->speed_ms) % act->len;
	int i;
	for (i=0; i<NUM_DIGITS; i++)
	{
		program_cell(
			act->board,
			NUM_DIGITS-1-i,
			ascii_to_bitmap(act->msg[(index+i)%act->len]));
	}
}
