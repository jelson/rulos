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

UIEventDisposition focus_input_handler(UIEventHandler *handler, UIEvent evt);

#define NO_CHILD (0xff)

void focus_init(FocusManager *act)
{
	act->func = focus_input_handler;
	cursor_init(&act->cursor);
	act->selectedChild = NO_CHILD;
	act->focusedChild = NO_CHILD;
	act->children_size = 0;
}

void focus_register(FocusManager *act, UIEventHandler *handler, RectRegion rr, char *label)
{
	uint8_t idx = act->children_size;
	assert(idx<NUM_CHILDREN);
	act->children[idx].handler = handler;
	act->children[idx].rr = rr;
	act->children[idx].label = label;
	act->children_size += 1;
}

UIEventDisposition focus_input_handler(UIEventHandler *raw_handler, UIEvent evt)
{
	FocusManager *fm = (FocusManager *) raw_handler;

	// avoid mod-by-zero
	if (fm->children_size == 0) { return uied_blur; }

	UIEventDisposition result = uied_accepted;
	uint8_t old_cursor_child = fm->selectedChild;
	if (fm->focusedChild != NO_CHILD)
	{
		UIEventDisposition uied;
		if (evt==evt_remote_escape)
		{
			uied = uied_blur;
		}
		else
		{
			UIEventHandler *childHandler = fm->children[fm->focusedChild].handler;
			uied = childHandler->func(childHandler, evt);
		}
		if (uied==uied_blur)
		{
			fm->selectedChild = fm->focusedChild;
			fm->focusedChild = NO_CHILD;
		}
	} else {
		// nobody focused. Start selectin'!
		switch (evt)
		{
			case uie_right:
				fm->selectedChild = (fm->selectedChild + 1 + fm->children_size) % fm->children_size;
				break;
			case uie_left:
				fm->selectedChild = (fm->selectedChild - 1 + fm->children_size) % fm->children_size;
				break;
			case uie_select:
				if (fm->selectedChild!=NO_CHILD)
				{
					// need to get cursor out of child's way,
					// before sending "focus" event to child,
					// so child can push his own cursor onto
					// the board_buffer stack.
					if (old_cursor_child != NO_CHILD)
					{
						cursor_hide(&fm->cursor);
						old_cursor_child = NO_CHILD;
					}

					fm->focusedChild = fm->selectedChild;
					fm->selectedChild = NO_CHILD;
					UIEventHandler *childHandler = fm->children[fm->focusedChild].handler;
					childHandler->func(childHandler, uie_focus);
				}
				break;
			case uie_focus:
				fm->selectedChild = 0;
				break;
			case uie_escape:	// escape -- yield focus to *my* parent
				fm->selectedChild = NO_CHILD;
				result = uied_blur;
				break;
			case evt_remote_escape:
				// shouldn't happen; if it does, it's that the network
				// is out of sync. Just drop it.
				break;
		}
	}
	uint8_t new_cursor_child = fm->selectedChild;
	if (new_cursor_child != old_cursor_child)
	{
		if (old_cursor_child != NO_CHILD)
		{
			cursor_hide(&fm->cursor);
		}
		if (new_cursor_child != NO_CHILD)
		{
			FocusChild *fc = &fm->children[new_cursor_child];
			cursor_set_label(&fm->cursor, fc->label);
			cursor_show(&fm->cursor, fc->rr);
		}
	}
	return result;
}

r_bool focus_is_active(FocusManager *act)
{
	return (act->selectedChild != NO_CHILD)
		|| (act->focusedChild != NO_CHILD);
}
