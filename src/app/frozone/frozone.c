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

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "rocket.h"
#include "clock.h"
#include "util.h"
#include "uart.h"
#include "animate.h"
#include "frobutton.h"

/****************************************************************************/

#define SYSTEM_CLOCK 500

UartState_t uart; 

int main()
{
	hal_init();
	init_clock(SYSTEM_CLOCK, TIMER0);

	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase

//	uart_init(&uart, 38400, TRUE, 0);

	Animate an;
	animate_init(&an);
//	animate_play(&an, Zap);

	Frobutton fb;
	frobutton_init(&fb, &an);

	cpumon_main_loop();

	return 0;
}
