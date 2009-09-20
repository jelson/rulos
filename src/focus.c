#include <inttypes.h>

#include "display_controller.h"
#include "focus.h"
#include "util.h"

void focus_update(FocusAct *act);

#define FOCUS_OFF (NUM_BOARDS)

void focus_init(FocusAct *act)
{
	act->func = (ActivationFunc) focus_update;
	board_buffer_init(&act->board_buffer);
	act->board = FOCUS_OFF;
	ascii_to_bitmap_str(act->board_buffer.buffer, "_CURSOR_");
	schedule(0, (Activation*) act);
}

void focus_update(FocusAct *act)
{
	if (act->board != FOCUS_OFF)
	{
		board_buffer_pop(&act->board_buffer);
	}
	// cycle through all boards plus an extra spot meaning "no focus"
	act->board = (act->board + 1) % (NUM_BOARDS + 1);
	if (act->board != FOCUS_OFF)
	{
		board_buffer_push(&act->board_buffer, act->board);
	}
	schedule(500, (Activation*) act);
}
