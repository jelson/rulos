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


#define LED_DRIVER_SDI_L   GPIO_A1
#define LED_DRIVER_SDI_R   GPIO_A0
#define LED_DRIVER_CLK     GPIO_A2
#define LED_DRIVER_LE      GPIO_A3
#define HEADLIGHT          GPIO_B2
#define TAILLIGHT         GPIO_B1

#define JIFFY_TIME_US 10000

typedef struct {
	r_bool tail_on;
	Time tail_next_toggle_time;
} BikeState_t;

#define TAIL_ON_TIME_MS  100
#define TAIL_OFF_TIME_MS 300

#if 0
static inline void clock()
{
	gpio_set(LED_DRIVER_CLK);
	gpio_clr(LED_DRIVER_CLK);
}
//
// This function shifts 8 bits into our 8-bit latch.
// The stove has only outputs 0 and 1 connected, so we shift real data
// into positions 0 and 1, and turn the rest off.
// 
// Each time a bit is shifted, it takes position 0, and pushing
// 0 through n-1 into positions 1 through n.  So, we shift 6 0's in first,
// then output 1, and finally output 0.
static void shift_in_config(BikeState_t* bike)
{
	gpio_clr(LED_DRIVER_LE);
	gpio_clr(LED_DRIVER_CLK);

	#if 0
	// shift in the real data 
	gpio_set_or_clr(LED_DRIVER_SDI, duktig->light2_on);
	clock();
	gpio_set_or_clr(LED_DRIVER_SDI, duktig->light1_on);
	clock();
	#endif
	
	// latch the register into the outputs
	gpio_set(LED_DRIVER_LE);
	gpio_clr(LED_DRIVER_LE);

	// tidy up (not actually necessary, but makes logic analyzer output easier to read)
	gpio_clr(LED_DRIVER_SDI_L);
	gpio_clr(LED_DRIVER_SDI_R);
}
#endif

static void bike_update(BikeState_t* bike)
{
	schedule_us(JIFFY_TIME_US, (ActivationFuncPtr) bike_update, bike);

	Time now = clock_time_us();
	
	// If we've reached the toy's timeout, shut down.
	if (later_than(now, bike->tail_next_toggle_time)) {
		if (bike->tail_on) {
			gpio_clr(TAILLIGHT);
			bike->tail_on = false;
			bike->tail_next_toggle_time = now + (TAIL_OFF_TIME_MS * (uint32_t) 1000);
		} else {
			gpio_set(TAILLIGHT);
			bike->tail_on = true;
			bike->tail_next_toggle_time = now + (TAIL_ON_TIME_MS * (uint32_t) 1000);
		}
	}
}


int main()
{
	hal_init();

	// set up output pins as drivers
	gpio_make_output(LED_DRIVER_SDI_L);
	gpio_make_output(LED_DRIVER_SDI_R);
	gpio_make_output(LED_DRIVER_CLK);
	gpio_make_output(LED_DRIVER_LE);
	gpio_make_output(HEADLIGHT);
	gpio_make_output(TAILLIGHT);

	// turn on the headlight
	gpio_set(HEADLIGHT);
	
	// init state
	BikeState_t bike;
	memset(&bike, 0, sizeof(bike));
	bike.tail_on = false;
	bike.tail_next_toggle_time = clock_time_us();
	
	// set up periodic update
	init_clock(JIFFY_TIME_US, TIMER1);
	schedule_now((ActivationFuncPtr) bike_update, &bike);
	
	cpumon_main_loop();
	assert(FALSE);
}

