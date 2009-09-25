#include "numeric_input.h"

void numeric_input_update(NumericInputAct *act);
UIEventDisposition numeric_input_handler(UIEventHandler *raw_handler, UIEvent evt);
void ni_update_once(NumericInputAct *act);
void ni_start_input(NumericInputAct *act);
void ni_accept_input(NumericInputAct *act);
void ni_cancel_input(NumericInputAct *act);
void ni_add_digit(NumericInputAct *act, uint8_t digit);

void numeric_input_init(NumericInputAct *act, RowRegion region, BoardBuffer *bbuf, FocusAct *fa)
{
	act->func = (ActivationFunc) numeric_input_update;
	act->region = region;
	act->bbuf = bbuf;
	act->handler.func = numeric_input_handler;
	act->handler.act = act;
	cursor_init(&act->cursor);
	cursor_set_shape_blank(&act->cursor, TRUE);
	act->cur_value.mantissa = 314;
	act->cur_value.neg_exponent = 2;
	act->decimal_present = TRUE;
	ni_update_once(act);
	DisplayRect rect = { bbuf->board_index, bbuf->board_index, region.x, region.x+region.xlen-1 };
	focus_register(fa, (UIEventHandler*) &act->handler, rect);
}

void numeric_input_update(NumericInputAct *act)
{
}

void ni_update_once(NumericInputAct *act)
{
	char str[10];
	int_to_string(str, act->region.xlen, FALSE, act->cur_value.mantissa);
	ascii_to_bitmap_str(act->region.bmp+act->region.x, act->region.xlen, str);
	if (act->decimal_present)
	{
		*(act->region.bmp+act->region.x+act->region.xlen-1-act->cur_value.neg_exponent) |= SSB_DECIMAL;
	}
	board_buffer_draw(act->bbuf);
}

void ni_start_input(NumericInputAct *act)
{
	act->old_value = act->cur_value;	// store in case we cancel
	act->cur_value.mantissa = 0;
	act->cur_value.neg_exponent = 0;
	uint8_t y = act->bbuf->board_index;
	uint8_t x = act->region.x+act->region.xlen-1;
	DisplayRect drect = {y,y,x,x};
	cursor_show(&act->cursor, drect);
	act->decimal_present = FALSE;
	ni_update_once(act);
}

void ni_accept_input(NumericInputAct *act)
{
	act->old_value = act->cur_value;
	cursor_hide(&act->cursor);
	act->decimal_present = TRUE;
	ni_update_once(act);
}

void ni_cancel_input(NumericInputAct *act)
{
	act->cur_value = act->old_value;
	cursor_hide(&act->cursor);
	act->decimal_present = TRUE;
	ni_update_once(act);
}

void ni_add_digit(NumericInputAct *act, uint8_t digit)
{
	if (act->cur_value.mantissa >= 10000)
	{
		// TODO hardcoded 4-wide field
		// ignore
		return;
	}
	act->cur_value.mantissa = (act->cur_value.mantissa*10) + digit;
	if (act->decimal_present)
	{
		act->cur_value.neg_exponent += 1;
	}
	ni_update_once(act);
}

UIEventDisposition numeric_input_handler(UIEventHandler *raw_handler, UIEvent evt)
{
	NumericInputAct *act = ((NumericInputHandler*) raw_handler)->act;
	switch (evt)
	{
		case uie_focus:
			ni_start_input(act);
			break;
		case 'c':	// select (TODO define symbol)
			ni_accept_input(act);
			// TODO should cause a blur
			break;
		case uie_blur:
			ni_cancel_input(act);
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			ni_add_digit(act, evt-'0');
			break;
		case 's':	// star = decimal point
			act->decimal_present = TRUE;
			ni_update_once(act);
			break;
	}
	return uied_ignore;
}
