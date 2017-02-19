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

#include "custom_board_defs.h"

/*
 * This callback is called every time we, as a TWI slave, get a query from
 * the TWI master. Each time that happens, we return whatever key is next
 * in the keypad buffer if one is queued, or 0 otherwise.
 */
static uint8_t get_next_char_to_transmit()
{
	return hal_read_keybuf();
}

int main()
{
	hal_init();

        // start clock with 10 msec resolution
	init_clock(10000, TIMER1);

	// start scanning the keypad
	hal_init_keypad();

	// Check the OPT pin. If it's open, use the default 7-bit address of
	// 0x32 (dec 50). If it's been jumpered, use 0x44 (dec 68).
	uint8_t gpio_address;
	gpio_make_input_enable_pullup(OPT_PIN);
	if (gpio_is_set(OPT_PIN)) {
		gpio_address = 0x32;
	} else {
		gpio_address = 0x44;
	}

	// set up a TWI slave handler for TWI queries
	usi_twi_slave_init(gpio_address, NULL, get_next_char_to_transmit);

	cpumon_main_loop();
	assert(FALSE);
}

