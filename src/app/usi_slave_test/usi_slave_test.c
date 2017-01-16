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

#include "rulos.h"
#include "hardware.h"
#include "clock.h"
#include "util.h"
#include "hal.h"
#include "usi_twi_slave.h"

uint8_t counter = 'a';

static uint8_t return_next_char()
{
	if (counter == 'z' + 1)
		counter = 'a';
	
	return counter++;
}

int main()
{
	gpio_make_output(GPIO_A5);
	
	hal_init();

        // start clock with 10 msec resolution
	init_clock(10000, TIMER1);

	// start the USI module, with a send func that returns an increasing
	// sequence of characters
	usi_twi_slave_init(50 /* address */, NULL, return_next_char);

	cpumon_main_loop();
	assert(FALSE);
}

