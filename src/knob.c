#include "util.h"
#include "knob.h"

UIEventDisposition knob_handler(UIEventHandler *raw_handler, UIEvent evt);
void knob_update_once(Knob *knob);

void knob_init(Knob *knob, RowRegion region, char **msgs, uint8_t len, FocusManager *fa)
{
	knob->func = knob_handler;
	knob->msgs = msgs;
	knob->len = len;
	knob->selected = 0;
	knob->region = region;
	cursor_init(&knob->cursor);
	cursor_set_shape_blank(&knob->cursor, TRUE);
	RectRegion rr = {&knob->region.bbuf, 1, knob->region.x, knob->region.xlen};
	focus_register(fa, (UIEventHandler*) knob, rr);
	knob_update_once(knob);
}

UIEventDisposition knob_handler(UIEventHandler *raw_handler, UIEvent evt)
{
	UIEventDisposition result = uied_accepted;
	Knob *knob = (Knob*) raw_handler;
	switch (evt)
	{
		case uie_focus:
		{
			RectRegion rr = {&knob->region.bbuf, 1, knob->region.x, knob->region.xlen};
			cursor_show(&knob->cursor, rr);
		}
			break;
		case uie_escape:
		case uie_select:
			cursor_hide(&knob->cursor);
			result = uied_blur;
			break;
		case uie_right:
			knob->selected = (knob->selected+1) % knob->len;
			knob_update_once(knob);
			break;
		case uie_left:
			knob->selected = (knob->selected-1+knob->len) % knob->len;
			knob_update_once(knob);
			break;
	}
	return result;
}

void knob_update_once(Knob *knob)
{
	ascii_to_bitmap_str(
		knob->region.bbuf->buffer+knob->region.x,
		knob->region.xlen, 
		knob->msgs[knob->selected]);
	board_buffer_draw(knob->region.bbuf);
}
