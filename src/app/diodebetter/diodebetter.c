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

/****************************************************************************/

#ifndef SIM

#include "hardware.h"
#include <avr/boot.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#define LEDR		GPIO_A7
#define LEDG		GPIO_B0
#define LEDB		GPIO_B1
#define LEDW		GPIO_B2

#define KNOB_HUE		0
#define KNOB_LIGHTNESS0	1
#define KNOB_LIGHTNESS1	2

//////////////////////////////////////////////////////////////////////////////

typedef struct {
	uint8_t hue;
	uint8_t lightness0;
	uint8_t lightness1;
} Inputs;

void inputs_init(Inputs* inputs)
{
	// note I'm NOT using init_adc(); it adds a scheduled function,
	// and we're not using any of rulos scheduling for this device.
	reg_set(&ADCSRA, ADEN); // enable ADC
//	reg_set(&ADCSRB, ADLAR); // enable left-aligned mode for 8-bit reads

	hal_init_adc_channel(KNOB_HUE);
	hal_init_adc_channel(KNOB_LIGHTNESS0);
	hal_init_adc_channel(KNOB_LIGHTNESS1);
}

//----------------------------------------------------------------------
// unexposed gunk copied out of lib/hardware_adc.c :vP
//
static void adc_busy_wait_conversion()
{
	reg_set(&ADCSRA, ADSC);
	while (reg_is_set(&ADCSRA, ADSC)) {}
}

static uint8_t read_adc_raw(uint8_t adc_channel)
{
	ADMUX = adc_channel;
	adc_busy_wait_conversion();
	// superstition:
	adc_busy_wait_conversion();

//	return ADCH;	// 8-bit variant
	uint16_t newval = ADCL;
	newval |= ((uint16_t) ADCH << 8);
	return newval;
}
//
//----------------------------------------------------------------------

void inputs_sample(Inputs* inputs)
{
	inputs->hue = read_adc_raw(KNOB_HUE);
	inputs->lightness0 = read_adc_raw(KNOB_LIGHTNESS0);
	inputs->lightness1 = read_adc_raw(KNOB_LIGHTNESS1);
}


//////////////////////////////////////////////////////////////////////////////
// Output control

typedef struct {
	uint8_t states[2];
	uint8_t duty;
} ControlChannel;

typedef struct {
	ControlChannel r;
	ControlChannel g;
	ControlChannel b;
	ControlChannel w;
} ControlTable;

inline void channel_configure(ControlChannel* channel, uint8_t value)
{
	if (value<128)
	{
		channel->states[0] = 1;
		channel->states[1] = 0;
		channel->duty = value;
	}
	else
	{
		channel->states[0] = 0;
		channel->states[1] = 1;
		channel->duty = value-128;
	}
}

static inline void channel_update(ControlChannel* channel, uint8_t t,
	volatile uint8_t *ddr, volatile uint8_t *port, volatile uint8_t *pin, uint8_t bit)
{
	uint8_t state_sel = (t<channel->duty) & 0x1;
	uint8_t v = channel->states[state_sel];

	// TODO This function doesn't have constant time:
	gpio_set_or_clr(ddr, port, pin, bit, v);
}

void control_init(ControlTable* table)
{
	gpio_make_output(LEDR);
	gpio_make_output(LEDG);
	gpio_make_output(LEDB);
	gpio_make_output(LEDW);
}

void control_phase(ControlTable* control_table)
{
	uint8_t t;
	for (t=0; t<128; t++)
	{
		channel_update(&control_table->r, t, LEDR);
		channel_update(&control_table->g, t, LEDG);
		channel_update(&control_table->b, t, LEDB);
		channel_update(&control_table->w, t, LEDW);
	}
}

//////////////////////////////////////////////////////////////////////////////

// stabilizing timer.
	// Each phase should take the same total amount of time,
	// so stall this phase to take the same time as an output phase.
	// 
	// Ballpark: 128 updates * 4 channels * 16 cycles per channel
	// --> 8000 cycles per half-period.
	// --> 16ms period @ 1MHz, 2ms @8MHz. That should do.

// prescalar constants on attiny84.pdf page 84, table 11-9.
#define TIMER_PRESCALER	(0x3)	/* clkIO/64. */
#define TIMER_COUNT		(128)

void timer_init()
{
	TCCR0A = 0;
	TCCR0B = TIMER_PRESCALER;
	OCR0A = TIMER_COUNT;
}

void timer_start()
{
	TCNT0 = 0;
	reg_clr(&TIFR0, OCF0A);
}

void timer_wait()
{
	while (reg_is_clr(&TIFR0, OCF0A))
	{}
}

//////////////////////////////////////////////////////////////////////////////
// Master phases

void hue_conversion(Inputs* inputs, ControlTable* control_table)
{
	uint16_t hue = (inputs->hue*3) >> 2;	// 0..767
#define FWD(x) (255-(x))
#define REV(x) (x)
	uint8_t r, g, b;
	if (hue<256)
	{
		r = FWD(hue);
		g = REV(hue);
		b = 0;
	}
	else if (hue<512)
	{
		g = FWD(hue-256);
		b = REV(hue-256);
		r = 0;
	}
	else
	{
		b = FWD(hue-512);
		r = REV(hue-512);
		g = 0;
	}
	// now scale by lightness.
	// I drop 10-bit lightness by 2 bits to avoid overflow.
	uint16_t color_lightness = inputs->lightness0 >> 2;
	r = (((uint16_t)r) * color_lightness) >> 8;
	g = (((uint16_t)r) * color_lightness) >> 8;
	b = (((uint16_t)r) * color_lightness) >> 8;

	uint8_t w = (inputs->lightness1) >> 2;

	channel_configure(&control_table->r, r);
	channel_configure(&control_table->g, g);
	channel_configure(&control_table->b, b);
	channel_configure(&control_table->w, w);
}
typedef struct {
	Inputs inputs;
	ControlTable control_table;
} App;

void app_init(App* app)
{
	timer_init();
	inputs_init(&app->inputs);
	control_init(&app->control_table);
}

void app_run(App* app)
{
	timer_start();
	inputs_sample(&app->inputs);
	hue_conversion(&app->inputs, &app->control_table);
	timer_wait();

	control_phase(&app->control_table);
}

int main()
{
	App app;
	app_init(&app);
	app_run(&app);
	return 0;
}

#else
int main()
{
	return 0;
}
#endif // SIM


