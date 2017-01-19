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
#include <stdbool.h>

#include "rocket.h"
#include "clock.h"
#include "util.h"
#include "network.h"
#include "sim.h"
#include "serial_console.h"


#if !SIM
#include "hardware.h"
#endif // !SIM


typedef struct {
	Activation act;
	uint8_t val;
	SerialConsole console;
} BlinkAct;

void _update_blink(Activation *act)
{
	BlinkAct *ba = (BlinkAct *) act;
	ba->val = (ba->val<<1)|(ba->val>>7);
#if !SIM
	PORTC = ba->val;
#endif

	serial_console_sync_send(&ba->console, "blink\n", 6);

	schedule_us(100000, &ba->act);
}

void blink_init(BlinkAct *ba)
{
	ba->act.func = _update_blink;
	serial_console_init(&ba->console, NULL);
	serial_console_sync_send(&ba->console, "init\n", 5);
	ba->val = 0x0f;

#if !SIM
	gpio_make_output(GPIO_C0);
	gpio_make_output(GPIO_C1);
	gpio_make_output(GPIO_C2);
	gpio_make_output(GPIO_C3);
	gpio_make_output(GPIO_C4);
	gpio_make_output(GPIO_C5);
	gpio_make_output(GPIO_C6);
	gpio_make_output(GPIO_C7);
#endif // !SIM

	schedule_us(100000, &ba->act);
}


int main()
{

	util_init();
	hal_init(bc_audioboard);	// TODO need a "bc_custom"
	init_clock(1000, TIMER1);

	BlinkAct ba;
	blink_init(&ba);

	CpumonAct cpumon;
	cpumon_init(&cpumon);	// includes slow calibration phase
	cpumon_main_loop();

	return 0;
}
