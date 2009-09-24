#include <inttypes.h>

#include "display_controller.h"
#include "focus.h"
#include "util.h"

void focus_input_handler(InputHandler *handler, char key);
//void focus_update(FocusAct *act);
//void focus_update_once(FocusAct *act);

#define NO_CHILD (0xff)

void focus_init(FocusAct *act)
{
	//act->func = (ActivationFunc) focus_update;
	cursor_init(&act->cursor);
	act->inputHandler.func = focus_input_handler;
	act->inputHandler.act = act;
	act->selectedChild = NO_CHILD;
	act->focusedChild = NO_CHILD;
	act->children_size = 0;
	//focus_update(act);
}

void focus_register(FocusAct *act, UIEventHandler *handler, uint8_t board)
{
	uint8_t idx = act->children_size;
	assert(idx<NUM_CHILDREN);
	act->children[idx].handler = handler;
	act->children[idx].board = board;
	act->children_size += 1;
}

/*
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
	if ((t >> 8) & 1)
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
*/

void focus_input_handler(InputHandler *raw_handler, char key)
{
	FocusAct *act = ((FocusInputHandler *) raw_handler)->act;

	// avoid mod-by-zero
	if (act->children_size == 0) { return; }

	uint8_t old_cursor_child = act->selectedChild;
	if (act->focusedChild != NO_CHILD)
	{
		UIEventHandler *childHandler = act->children[act->focusedChild].handler;
		if (key=='d') { // escape
			childHandler->func(childHandler, uie_blur);
			act->selectedChild = act->focusedChild;
			act->focusedChild = NO_CHILD;
		}
		else // pass along key
		{
			childHandler->func(childHandler, key);
		}
	} else {
		// nobody focused. Start selectin'!
		switch (key)
		{
			case 'a':	// right
				act->selectedChild = (act->selectedChild + 1 + act->children_size) % act->children_size;
				break;
			case 'b':	// left
				act->selectedChild = (act->selectedChild - 1 + act->children_size) % act->children_size;
				break;
			case 'c':	// select
				if (act->selectedChild!=NO_CHILD)
				{
					act->focusedChild = act->selectedChild;
					act->selectedChild = NO_CHILD;
					UIEventHandler *childHandler = act->children[act->focusedChild].handler;
					childHandler->func(childHandler, uie_focus);
				}
				break;
			case 'd':	// escape
				act->selectedChild = NO_CHILD;
		}
	}
	uint8_t new_cursor_child = act->selectedChild;
	if (new_cursor_child != old_cursor_child)
	{
		if (old_cursor_child != NO_CHILD)
		{
			//LOGF((logfp, "pop child %d\n", old_cursor_child));
			//board_buffer_pop(&act->board_buffer);
			cursor_hide(&act->cursor);
		}
		if (new_cursor_child != NO_CHILD)
		{
			LOGF((logfp, "push child %d\n", new_cursor_child));
			cursor_show(&act->cursor,
				act->children[new_cursor_child].board,
				act->children[new_cursor_child].board,
				1, 7);
		}
		//focus_update_once(act);
	}
}

