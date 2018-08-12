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

#include "periph/rocket/potsticker.h"

void ps_update(PotSticker *ps)
{
	int16_t new_val = hal_read_adc(ps->adc_channel);
	int8_t new_left = (new_val+ps->hysteresis)/ps->detents;
	int8_t new_right = (new_val-ps->hysteresis)/ps->detents;

#if DEBUG_BRUTE_DISPLAY
	static char dbg_v = '_';
#endif

	if (new_left < ps->last_digital_value)
	{
		ps->ifi->func(ps->ifi, ps->back);
		ps->last_digital_value = new_left;
#if DEBUG_BRUTE_DISPLAY
		dbg_v = 'L';
#endif
	}
	else if (new_right > ps->last_digital_value)
	{
		ps->ifi->func(ps->ifi, ps->fwd);
		ps->last_digital_value = new_right;
#if DEBUG_BRUTE_DISPLAY
		dbg_v = 'R';
#endif
	}

#if DEBUG_BRUTE_DISPLAY
	char msg[8];
	int_to_string2(msg, 4, 0, ps->last_digital_value);
	msg[4] = dbg_v;
	int_to_string2(&msg[5], 3, 0, new_left);
	SSBitmap bm[8];
	ascii_to_bitmap_str(bm, 8, msg);
	program_board(2, bm);
#endif // DEBUG_BRUTE_DISPLAY

	schedule_us(100, (ActivationFuncPtr) ps_update, ps);
}

void init_potsticker(PotSticker *ps, uint8_t adc_channel, InputInjectorIfc *ifi, uint8_t detents, char fwd, char back)
{
	ps->adc_channel = adc_channel;
	hal_init_adc_channel(ps->adc_channel);
	ps->ifi = ifi;
	ps->detents = detents;
	ps->hysteresis = 1024/detents/4;
	ps->fwd = fwd;
	ps->back = back;

	ps->last_digital_value = hal_read_adc(ps->adc_channel) / ps->detents;

	schedule_us(1, (ActivationFuncPtr) ps_update, ps);
}
