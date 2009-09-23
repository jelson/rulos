#include <inttypes.h>

#include "display_controller.h"
#include "focus.h"
#include "util.h"

void focus_update(FocusAct *act);
void focus_input_handler(InputHandler *handler, char key);
void focus_update_once(FocusAct *act);

#define FOCUS_OFF (0xff)

void focus_init(FocusAct *act)
{
	act->func = (ActivationFunc) focus_update;
	board_buffer_init(&act->board_buffer);
	act->board = FOCUS_OFF;
	act->inputHandler.func = focus_input_handler;
	act->inputHandler.act = act;
	act->childHandler = NULL;
	act->selectUntilTime = 0;
	focus_update(act);
}

void focus_update(FocusAct *act)
{
	focus_update_once(act);
	// I happen to "know" that 250ms is the next time anything "happens"
	// (select disappears, or cursor blinks.)
	schedule(250, (Activation*) act);
}

void focus_update_once(FocusAct *act)
{
	uint32_t t = clock_time();
	SSBitmap *buf = act->board_buffer.buffer;
	if (t < act->selectUntilTime)
	{
		ascii_to_bitmap_str(buf, "{select}");
	}
	else if ((t >> 8) & 1)
	{
		buf[0] = 0b1111000;
		buf[1] = 0b0000000;
		buf[2] = 0b0000000;
		buf[3] = 0b0000000;
		buf[4] = 0b0000000;
		buf[5] = 0b0000000;
		buf[6] = 0b0000000;
		buf[7] = 0b1001110;
	}
	else
	{
		buf[0] = 0b1111001;
		buf[1] = 0b0000001;
		buf[2] = 0b0000001;
		buf[3] = 0b0000001;
		buf[4] = 0b0000001;
		buf[5] = 0b0000001;
		buf[6] = 0b0000001;
		buf[7] = 0b1001111;
	}
	board_buffer_draw(&act->board_buffer);
}

void focus_input_handler(InputHandler *raw_handler, char key)
{
	FocusAct *act = ((FocusInputHandler *) raw_handler)->act;
	act->selectUntilTime = 0;
	if (act->childHandler != NULL)
	{
		act->childHandler->func(act->childHandler, key);
		return;
	}
	uint8_t new_board = act->board;
	switch (key)
	{
		case 'a':	// right
			new_board = (act->board + 1 + NUM_BOARDS) % NUM_BOARDS;
			break;
		case 'b':	// left
			new_board = (act->board - 1 + NUM_BOARDS) % NUM_BOARDS;
			break;
		case 'c':	// select
			if (act->board < NUM_BOARDS)
			{
				// TODO pass off to child handler
				// for now, temporarily change display text.
				act->selectUntilTime = clock_time() + 1500;
				focus_update_once(act);
			}
			break;
		case 'd':	// escape
			new_board = FOCUS_OFF;
			break;
	}
	if (new_board != act->board)
	{
		if (act->board != FOCUS_OFF) {
			board_buffer_pop(&act->board_buffer);
		}
		act->board = new_board;
		if (act->board != FOCUS_OFF) {
			board_buffer_push(&act->board_buffer, act->board);
		}
		focus_update_once(act);
	}
}


