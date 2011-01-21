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
#include "network.h"
#include "sim.h"
#include "audio_driver.h"
#include "audio_server.h"
#include "audio_streamer.h"
#include "sdcard.h"

#if !SIM
#include "hardware.h"
#endif // !SIM

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	Activation act;
	uint8_t val;
} BlinkAct;

void _update_blink(Activation *act)
{
	BlinkAct *ba = (BlinkAct *) act;
	ba->val += 1;
#if !SIM
	gpio_set_or_clr(GPIO_B0, ((ba->val>>0)&0x1)==0);
	gpio_set_or_clr(GPIO_B1, ((ba->val>>1)&0x1)==0);
	gpio_set_or_clr(GPIO_B2, ((ba->val>>2)&0x1)==0);
	gpio_set_or_clr(GPIO_B3, ((ba->val>>3)&0x1)==0);
	gpio_set_or_clr(GPIO_D3, ((ba->val>>4)&0x1)==0);
	gpio_set_or_clr(GPIO_D4, ((ba->val>>5)&0x1)==0);
	gpio_set_or_clr(GPIO_D5, ((ba->val>>6)&0x1)==0);
	gpio_set_or_clr(GPIO_D6, ((ba->val>>7)&0x1)==0);
#endif // !SIM
	schedule_us(100000, &ba->act);
}

void blink_init(BlinkAct *ba)
{
	ba->act.func = _update_blink;
	ba->val = 0;
#if !SIM
	gpio_make_output(GPIO_B0);
	gpio_make_output(GPIO_B1);
	gpio_make_output(GPIO_B2);
	gpio_make_output(GPIO_B3);
	gpio_make_output(GPIO_D3);
	gpio_make_output(GPIO_D4);
	gpio_make_output(GPIO_D5);
	gpio_make_output(GPIO_D6);
#endif // !SIM
	schedule_us(100000, &ba->act);
}

//////////////////////////////////////////////////////////////////////////////

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

