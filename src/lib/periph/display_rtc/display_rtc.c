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

#include "core/rulos.h"
#include "periph/input_controller/input_controller.h"
#include "periph/display_rtc/display_rtc.h"

void drtc_update(DRTCAct *act);
void drtc_update_once(DRTCAct *act);

void drtc_init(DRTCAct *act, uint8_t board, Time base_time)
{
	act->base_time = base_time;
	board_buffer_init(&act->bbuf DBG_BBUF_LABEL("rtc"));
	board_buffer_push(&act->bbuf, board);
	schedule_us(1, (ActivationFuncPtr) drtc_update, act);
}

void drtc_update(DRTCAct *act)
{
	schedule_us(Exp2Time(15), (ActivationFuncPtr) drtc_update, act);
	drtc_update_once(act);
}

void drtc_update_once(DRTCAct *act)
{
	Time c = (clock_time_us() - act->base_time)/1000;

	char buf[16], *p = buf;
	p += int_to_string2(p, 8, 3, c/10);
	buf[0] = 't';

	ascii_to_bitmap_str(act->bbuf.buffer, NUM_DIGITS, buf);
	// jonh hard-codes decimal point
	act->bbuf.buffer[5] |= SSB_DECIMAL;
	board_buffer_draw(&act->bbuf);
}

void drtc_set_base_time(DRTCAct *act, Time base_time)
{
	act->base_time = base_time;
	drtc_update_once(act);
}

Time drtc_get_base_time(DRTCAct *act)
{
	return act->base_time;
}
