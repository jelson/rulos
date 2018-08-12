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

#include "core/rulos.h"
#include "core/hal.h"

#define JIFFY_TIME_US 10000
#define BUTTON_REFRAC_TIME_US 100000
#define FOUNTAIN_ON_TIME_SEC 60
#define FOUNTAIN_FAST_TIME_MSEC 1000
#define PWM

typedef struct {
	DebouncedButton_t button;
	r_bool fountain_on;
	r_bool fountain_fast;
	Time fountain_slowdown_time;
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

void hal_slowdown_pump()
{
	printf("pump slowing down\n");
}

void hal_stop_pump()
{
	printf("pump now off\n");
}

#else // SIM

#include "hardware.h"

#define BUTTON GPIO_B2
#define PUMP   GPIO_B1

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

#ifdef PWM
void activate_pwm(int duty_cycle_percent)
{
	OCR1A = duty_cycle_percent;
	OCR1C = 100;
	TCCR1 = 0
		| _BV(CTC1)   // Reset timer 1 when it hits OCR1C
		| _BV(PWM1A)  // Enable PWM from OCR1A
		| _BV(COM1A1) // Set OC1A at 0, clear at OC1C
		| _BV(CS10) // No prescale
		;
}
#endif

void hal_start_pump()
{
#ifdef PWM
	activate_pwm(100);
#else
	gpio_set(PUMP);
#endif
}

void hal_slowdown_pump()
{
#ifdef PWM
	activate_pwm(15);
#endif
}

void hal_stop_pump()
{
#ifdef PWM
	TCCR1 = 0;
	gpio_clr(PUMP);
#else
	gpio_clr(PUMP);
#endif
}

#endif // SIM


void start_fountain(FountainState_t* f)
{
	f->fountain_on = TRUE;
	f->fountain_fast = TRUE;
	f->fountain_slowdown_time = clock_time_us() +
		1000 * ((Time) FOUNTAIN_FAST_TIME_MSEC);
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

	// If we've finished fast-mode, slow down the pump.
	if (f->fountain_on && f->fountain_fast &&
	    later_than_or_eq(clock_time_us(), f->fountain_slowdown_time)) {
		f->fountain_fast = FALSE;
		hal_slowdown_pump();
	}
	
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

