#include <stdio.h>
#include <inttypes.h>

#include "display_controller.h"
#include "display_rtc.h"
#include "util.h"

Activation drtc_activation = { drtc_update };

void drtc_init()
{
	schedule(0, &drtc_activation);
}

void drtc_update()
{
	uint32_t c = clock_time();
	char buf[16], *p = buf;
	*p++ = 't';
	p += int_to_string(p, 5, FALSE, c/1000);
	*p++ = '.';
	p += int_to_string(p, 2, TRUE, (c%1000)/10);
	
	program_string(0, " clock  ");
	program_string(1, buf);
	schedule(30, &drtc_activation);
}

