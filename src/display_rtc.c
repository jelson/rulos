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
	p += int_to_string(p, 4, c/1000);
	*p++ = '.';
	p += int_to_string(p, 0, (c%1000)/10);
	
	program_string(0, " clock  ");
	program_string(1, buf);
	schedule(50, &drtc_activation);
}

#if 0
/* This doesn't work on the microcontroller ... perhaps snprintf is screwed? */
void drtc_update()
{
	char buf[10];
	double c = clock_time() / 1000.0;
	snprintf(buf, 9, "t%7.2f", c);
	program_string(0, " clock  ");
	program_string(1, buf);
	schedule(150, &drtc_activation);
	//say("drtc_update\n");
}
#endif

