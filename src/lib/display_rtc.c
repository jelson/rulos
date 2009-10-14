#include <stdio.h>
#include <inttypes.h>

#include "rocket.h"
#include "display_rtc.h"

void drtc_update(DRTCAct *act);

void drtc_init(DRTCAct *act, uint8_t board, Time base_time)
{
	act->func = (ActivationFunc) drtc_update;
	act->base_time = base_time;
	board_buffer_init(&act->bbuf);
	board_buffer_push(&act->bbuf, board);
	schedule_us(1, (Activation*) act);
}

void drtc_update(DRTCAct *act)
{
	Time c = (clock_time_us() - act->base_time)/1000;

	char buf[16], *p = buf;
	p += int_to_string2(p, 8, 3, c/10);
	buf[0] = 't';

	ascii_to_bitmap_str(act->bbuf.buffer, NUM_DIGITS, buf);
	// jonh hard-codes decimal point
	act->bbuf.buffer[5] |= SSB_DECIMAL;
	board_buffer_draw(&act->bbuf);
	schedule_us(Exp2Time(15), (Activation*) act);
}

void drtc_set_base_time(DRTCAct *act, Time base_time)
{
	act->base_time = base_time;
	drtc_update(act);
}
