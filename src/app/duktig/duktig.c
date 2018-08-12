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

#include "core/clock.h"
#include "core/rulos.h"
#include "core/util.h"
#include "hardware.h"


#define LED_DRIVER_SDI   GPIO_A6
#define LED_DRIVER_CLK   GPIO_A4
#define LED_DRIVER_LE    GPIO_A3
#define LED_DRIVER_OE    GPIO_A5
#define POWER_GATE       GPIO_A2
#define BUT1             GPIO_A0
#define BUT2             GPIO_A1

#define JIFFY_TIME_US 10000
#define KEY_REFRACTORY_TIME_US 100000
#define TIMEOUT_WHILE_ON_US  (1000000*120) // 120 sec
#define TIMEOUT_WHILE_OFF_US (1000000*1) // 1 sec

typedef struct {
	r_bool light1_on;
	r_bool light2_on;
	DebouncedButton_t but1;
	DebouncedButton_t but2;
	Time shutdown_time;
} DuktigState_t;


static void set_timeout(DuktigState_t *duktig, Time timeout)
{
	duktig->shutdown_time = clock_time_us() + timeout;
}


// Set the power state of the toy.  Power on means turn on power to
// both the power gate and the LED driver's output enable (which is
// active low).
static void set_power(r_bool onoff)
{
	if (onoff) {
		gpio_make_output(POWER_GATE);
		gpio_set(POWER_GATE);
		gpio_clr(LED_DRIVER_OE);
	} else {
		gpio_make_input_disable_pullup(POWER_GATE);
		gpio_set(LED_DRIVER_OE);
	}
}
	
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
static void shift_in_config(DuktigState_t *duktig)
{
	gpio_clr(LED_DRIVER_LE);
	gpio_clr(LED_DRIVER_CLK);

	// shift in 6 bits of "off" 
	gpio_clr(LED_DRIVER_SDI);
	clock();
	clock();
	clock();
	clock();
	clock();
	clock();

	// shift in the real data 
	gpio_set_or_clr(LED_DRIVER_SDI, duktig->light2_on);
	clock();
	gpio_set_or_clr(LED_DRIVER_SDI, duktig->light1_on);
	clock();

	// latch the register into the outputs
	gpio_set(LED_DRIVER_LE);
	gpio_clr(LED_DRIVER_LE);

	// tidy up (not actually necessary, but makes logic analyzer output easier to read)
	gpio_clr(LED_DRIVER_SDI);
}


static void duktig_update(DuktigState_t *duktig)
{
	schedule_us(JIFFY_TIME_US, (ActivationFuncPtr) duktig_update, duktig);

	r_bool but1_pushed = debounce_button(&(duktig->but1), gpio_is_set(BUT1));
	r_bool but2_pushed = debounce_button(&(duktig->but2), gpio_is_set(BUT2));

	if (but1_pushed) {
		duktig->light1_on = !duktig->light1_on;
	}

	if (but2_pushed) {
		duktig->light2_on = !duktig->light2_on;
	}

	// If a button was just pushed, send the new light state to the LED driver
	if (but1_pushed || but2_pushed) {
		// enable outputs -- just in case a button push came after we tried
		// to turn ourselves off, but before power actually died.
		set_power(1);

		// reconfigure led driver
		shift_in_config(duktig);

		// if we just turned the lights on, set a long timeout; if we just turned them off,
		// set a short one
		if (duktig->light1_on || duktig->light2_on) {
			set_timeout(duktig, TIMEOUT_WHILE_ON_US);
		} else {
			set_timeout(duktig, TIMEOUT_WHILE_OFF_US);
		}
	}

	// If we've reached the toy's timeout, shut down.
	if (later_than(clock_time_us(), duktig->shutdown_time)) {
		set_power(0);
	}
}


int main()
{
	hal_init();

	// set up output pins as drivers
	gpio_make_output(LED_DRIVER_SDI);
	gpio_make_output(LED_DRIVER_CLK);
	gpio_make_output(LED_DRIVER_LE);
	gpio_make_output(LED_DRIVER_OE);

	// turn on power to LEDs and ourselves
	set_power(1);

	// init state
	DuktigState_t duktig;
	memset(&duktig, 0, sizeof(duktig));
	set_timeout(&duktig, TIMEOUT_WHILE_ON_US);

	// set up buttons
	gpio_make_input_disable_pullup(BUT1);
	debounce_button_init(&duktig.but1, KEY_REFRACTORY_TIME_US);
	gpio_make_input_disable_pullup(BUT2);
	debounce_button_init(&duktig.but2, KEY_REFRACTORY_TIME_US);

	// set up periodic sampling task
	init_clock(JIFFY_TIME_US, TIMER1);
	schedule_us(1, (ActivationFuncPtr) duktig_update, &duktig);
	
	cpumon_main_loop();
	assert(FALSE);
}

