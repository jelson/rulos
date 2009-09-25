#include <inttypes.h>

#include "display_controller.h"
#include "focus.h"
#include "util.h"

void focus_input_handler(InputHandler *handler, char key);

#define NO_CHILD (0xff)

void focus_init(FocusAct *act)
{
	cursor_init(&act->cursor);
	act->inputHandler.func = focus_input_handler;
	act->inputHandler.act = act;
	act->selectedChild = NO_CHILD;
	act->focusedChild = NO_CHILD;
	act->children_size = 0;
}

void focus_register(FocusAct *act, UIEventHandler *handler, RectRegion rr)
{
	uint8_t idx = act->children_size;
	assert(idx<NUM_CHILDREN);
	act->children[idx].handler = handler;
	act->children[idx].rr = rr;
	act->children_size += 1;
}

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
					// need to get cursor out of child's way
					if (old_cursor_child != NO_CHILD)
					{
						cursor_hide(&act->cursor);
						old_cursor_child = NO_CHILD;
					}

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
			cursor_hide(&act->cursor);
		}
		if (new_cursor_child != NO_CHILD)
		{
			cursor_show(&act->cursor,
				act->children[new_cursor_child].rr);
		}
	}
}

