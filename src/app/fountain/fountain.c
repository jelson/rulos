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

#include "rulos.h"
#include "hal.h"

#define JIFFY_TIME_US 10000
#define BUTTON_REFRAC_TIME_US 500000
#define FOUNTAIN_ON_TIME_SEC 5

typedef struct {
	DebouncedButton_t button;
	r_bool fountain_on;
	Time fountain_stop_time;
} FountainState_t;

#ifdef SIM

void hal_fountain_init()
{
}

int button_counter = 0;
int down = 0;

r_bool hal_button_pressed()
{
	if (++button_counter == 400) {
		down = !down;
		button_counter = 0;
		printf("%spressing button\n", down ? "" : "un");
	}

	return down;
}

void hal_start_pump()
{
	printf("pump now on\n");
}

void hal_stop_pump()
{
	printf("pump now off\n");
}

#else // SIM

#include "hardware.h"

#define BUTTON GPIO_B2
#define PUMP   GPIO_B0

void hal_fountain_init()
{
	// Make the button pin an input with pullup disabled; we assume there's
	// an external pulldown resistor. We have to use a pulldown instead of
	// a pullup because the fountain's button has power for an internal LED,
	// and when pushed, it connects the power to the switched output.
	gpio_make_input_disable_pullup(BUTTON);

	// Make the pump controller pin an output.
	gpio_make_output(PUMP);
}

r_bool hal_button_pressed()
{
	return gpio_is_set(BUTTON);
}

void hal_start_pump()
{
	gpio_set(PUMP);
}

void hal_stop_pump()
{
	gpio_clr(PUMP);
}

#endif // SIM


void start_fountain(FountainState_t* f)
{
	f->fountain_on = TRUE;
	f->fountain_stop_time = clock_time_us() +
		1000000 * ((Time) FOUNTAIN_ON_TIME_SEC);
	hal_start_pump();
}

void stop_fountain(FountainState_t* f)
{
	f->fountain_on = FALSE;
	hal_stop_pump();
}

static void fountain_update(FountainState_t* f)
{
	schedule_us(JIFFY_TIME_US, (ActivationFuncPtr)fountain_update, f);

	// If the fountain is on and has timed out, turn it off.
	if (f->fountain_on &&
	    later_than_or_eq(clock_time_us(), f->fountain_stop_time)) {
		stop_fountain(f);
	}

	// Debounce the button and determine if we just experienced a button press.
	r_bool button_pressed = debounce_button(&f->button, hal_button_pressed());

	if (!button_pressed) {
		return;
	}

	// If the fountain was already on, turn it off, and vice-versa.
	if (f->fountain_on) {
		stop_fountain(f);
	} else {
		start_fountain(f);
	}
}


int main()
{
	
	FountainState_t f;
	
	hal_init();
	hal_fountain_init();

	debounce_button_init(&f.button, BUTTON_REFRAC_TIME_US);

	stop_fountain(&f);
	
        // Start clock.
	init_clock(JIFFY_TIME_US, TIMER0);
	schedule_now((ActivationFuncPtr)fountain_update, &f);
	cpumon_main_loop();

	assert(FALSE);
}

