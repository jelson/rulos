#include <stdio.h>

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
	char buf[32];
	snprintf(buf, 9, "t%7.2f        ", clock_time()/1000.0);
	program_string(0, " clock  ");
	program_string(1, buf);
	schedule(150, &drtc_activation);
	//say("drtc_update\n");
}
