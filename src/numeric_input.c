#include "numeric_input.h"

uint8_t dfp_draw(DecimalFloatingPoint *dfp, SSBitmap *bm, uint8_t len, uint8_t show_decimal)
{
	uint8_t width = dfp->neg_exponent + 1;
	uint8_t mantissa_width = 0;
	uint16_t mant_copy = dfp->mantissa;
	while (mant_copy>0)
	{
		mantissa_width += 1;
		mant_copy = mant_copy / 10;
	}
	if (mantissa_width > width) { width = mantissa_width; }
	if (bm!=NULL) {
		uint8_t d;
		// paste in width zeros, so that any leading zeros are present
		for (d=0; d<len; d++)
		{
			bm[len-1-d] = d < width ? SEVSEG_0 : 0;
		}
		// now draw in the mantissa digits
		for (d=0, mant_copy=dfp->mantissa; mant_copy>0; d++, mant_copy=mant_copy/10)
		{
			bm[len-1-d] = ascii_to_bitmap('0'+(mant_copy%10));
		}
		assert(d<=width);
		// draw in the decimal point
		if (show_decimal)
		{
			bm[len-1-dfp->neg_exponent] |= SSB_DECIMAL;
		}
	}
	return width;
}

void numeric_input_update(NumericInputAct *act);
UIEventDisposition numeric_input_handler(UIEventHandler *raw_handler, UIEvent evt);
void ni_update_once(NumericInputAct *act);
void ni_start_input(NumericInputAct *act);
void ni_accept_input(NumericInputAct *act);
void ni_cancel_input(NumericInputAct *act);
void ni_add_digit(NumericInputAct *act, uint8_t digit);

void numeric_input_init(NumericInputAct *act, RowRegion region, FocusAct *fa)
{
	act->func = (ActivationFunc) numeric_input_update;
	act->region = region;
	act->handler.func = numeric_input_handler;
	act->handler.act = act;
	cursor_init(&act->cursor);
	cursor_set_shape_blank(&act->cursor, TRUE);
	act->cur_value.mantissa = 314;
	act->cur_value.neg_exponent = 2;
	act->decimal_present = TRUE;
	ni_update_once(act);
	RectRegion rr = { &act->region.bbuf, 1, region.x, region.xlen };
	focus_register(fa, (UIEventHandler*) &act->handler, rr);
}

void numeric_input_update(NumericInputAct *act)
{
}

void ni_update_once(NumericInputAct *act)
{
	dfp_draw(&act->cur_value, act->region.bbuf->buffer+act->region.x, act->region.xlen, act->decimal_present);
	board_buffer_draw(act->region.bbuf);
}

void ni_start_input(NumericInputAct *act)
{
	act->old_value = act->cur_value;	// store in case we cancel
	act->cur_value.mantissa = 0;
	act->cur_value.neg_exponent = 0;

	RectRegion rr = { &act->region.bbuf, 1, act->region.x, act->region.xlen };
	cursor_show(&act->cursor, rr);
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
	DecimalFloatingPoint new_value;
	new_value.mantissa = act->cur_value.mantissa*10+digit;
	new_value.neg_exponent = act->cur_value.neg_exponent;
	if (act->decimal_present)
	{
		new_value.neg_exponent += 1;
	}
	uint8_t len = dfp_draw(&new_value, NULL, act->region.xlen, FALSE);
	if (len > act->region.xlen)
	{
		// number too big for display
		return;
	}

	// number adequate; keep it.
	act->cur_value = new_value;
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
			act->decimal_present = TRUE;	// never takes space
			ni_update_once(act);
			break;
	}
	return uied_ignore;
}
