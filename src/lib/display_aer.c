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
#include "display_aer.h"


void daer_update(DAER *act);
void daer_fetchCalcDecorationValues(
	struct s_decoration_ifc *daer_decoration_ifc,
	DecimalFloatingPoint *op0, DecimalFloatingPoint *op1);

void daer_init(DAER *daer, uint8_t board, Time impulse_frequency_us)
{
	board_buffer_init(&daer->bbuf DBG_BBUF_LABEL("aer"));
	board_buffer_push(&daer->bbuf, board);
	drift_anim_init(&daer->azimuth,   10, 320,   0, 360, 5);
	drift_anim_init(&daer->elevation, 10,   4,   0,  90, 5);
	drift_anim_init(&daer->roll,      10, -90, -90,  90, 5);
	daer->impulse_frequency_us = impulse_frequency_us;
	daer->last_impulse = 0;

	daer->decoration_ifc.func = (FetchCalcDecorationValuesFunc) daer_fetchCalcDecorationValues;
	daer->decoration_ifc.daer = daer;

	schedule_us(1, (ActivationFuncPtr) daer_update, daer);
}

void daer_update(DAER *daer)
{
	schedule_us(Exp2Time(16), (ActivationFuncPtr) daer_update, daer);

	Time t = clock_time_us();
	if (t - daer->last_impulse > daer->impulse_frequency_us)
	{
		da_random_impulse(&daer->azimuth);
		da_random_impulse(&daer->elevation);
		da_random_impulse(&daer->roll);
		daer->last_impulse = t;
	}

	char str[9];
	int_to_string2(str+0, 3, 3, da_read(&daer->azimuth));
	int_to_string2(str+3, 2, 2, da_read(&daer->elevation));
	int_to_string2(str+5, 3, 2, da_read(&daer->roll));

	ascii_to_bitmap_str(daer->bbuf.buffer, 8, str);
	board_buffer_draw(&daer->bbuf);
}

void daer_fetchCalcDecorationValues(
	struct s_decoration_ifc *daer_decoration_ifc,
	DecimalFloatingPoint *op0, DecimalFloatingPoint *op1)
{
	DAER *daer = daer_decoration_ifc->daer;
	op0->mantissa = da_read(&daer->azimuth)*10;
	op0->neg_exponent = 1;
	op1->mantissa = da_read(&daer->elevation)*100;
	op1->neg_exponent = 2;
}

