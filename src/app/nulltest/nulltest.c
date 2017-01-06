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

#include "rulos.h"

#define FREQ_USEC 50000

void test_func(void *data)
{
	schedule_us(FREQ_USEC, (ActivationFuncPtr) test_func, NULL);
}


int main()
{
	hal_init();

	init_clock(FREQ_USEC, TIMER1);

	schedule_now((ActivationFuncPtr) test_func, NULL);

	cpumon_main_loop();
}
