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

#define LIGHT_PIN GPIO_B3
#define TIMEOUT_MINUTES 30

/////////////////////////////

#include "core/rulos.h"
#include "hardware.h"
#include "chip/avr/periph/usi_serial/usi_serial.h"

uint16_t seconds_on = 0;

const uint16_t off_time = TIMEOUT_MINUTES * 60;
#define ONE_SEC_IN_USEC 1000000

static void one_sec(void *data)
{
	usi_serial_send("S");

	if (seconds_on < off_time) {
		seconds_on++;
		schedule_us(ONE_SEC_IN_USEC, (ActivationFuncPtr) one_sec, NULL);
	} else {
		gpio_clr(LIGHT_PIN);
	}
}

int main()
{
	hal_init();

	gpio_make_output(LIGHT_PIN);
	gpio_set(LIGHT_PIN);

#ifdef TIMING_DEBUG_PIN
	gpio_make_output(TIMING_DEBUG_PIN);

	for (int i = 0; i < 20; i++) {
		gpio_set(TIMING_DEBUG_PIN);
		gpio_clr(TIMING_DEBUG_PIN);
}
#endif

	usi_serial_send("A");

	init_clock(10000, TIMER0);
	schedule_now((ActivationFuncPtr) one_sec, NULL);

	usi_serial_send("L");

	cpumon_main_loop();
}
