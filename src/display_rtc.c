#include <stdio.h>
#include <inttypes.h>

#include "display_controller.h"
#include "display_rtc.h"
#include "util.h"

void drtc_update(DRTCAct *act);

void drtc_init(DRTCAct *act, uint8_t board)
{
	act->func = (ActivationFunc) drtc_update;
	board_buffer_init(&act->bbuf);
	board_buffer_push(&act->bbuf, board);
	schedule(1, (Activation*) act);
}

void drtc_update(DRTCAct *act)
{
	uint32_t c = clock_time();
	char buf[16], *p = buf;
	*p++ = 't';
	p += int_to_string(p, 5, FALSE, c/1000);
	//*p++ = '.';
	p += int_to_string(p, 2, TRUE, (c%1000)/10);
	
	ascii_to_bitmap_str(act->bbuf.buffer, NUM_DIGITS, buf);
	// jonh hard-codes decimal point
	act->bbuf.buffer[5] |= SSB_DECIMAL;
	board_buffer_draw(&act->bbuf);
	schedule(30, (Activation*) act);
}

