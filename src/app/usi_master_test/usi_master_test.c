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

#include <ctype.h>
#include <inttypes.h>

#include "chip/avr/periph/usi_twi_master/usi_twi_master.h"
#include "core/clock.h"
#include "core/hal.h"
#include "core/rulos.h"
#include "core/util.h"
#include "hardware.h"


int main()
{
	hal_init();

        // start clock with 10 msec resolution
	init_clock(10000, TIMER1);

	static char* test = "hellotest";
	usi_twi_master_init();
	usi_twi_master_sendorrecv(0x44, test, strlen(test));

	cpumon_main_loop();
	assert(FALSE);
}

