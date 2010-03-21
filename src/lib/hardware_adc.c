/*
 * hardware_adc.c: code for talking to ADC hardware
 */

#include <avr/boot.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay_basic.h>

#include "rocket.h"
#include "hardware.h"
#include "board_defs.h"
#include "hal.h"

#define NUM_ADCS 6
// #define RAW_ADC


typedef struct {
	ActivationFunc func;
	r_bool enable[NUM_ADCS];
	volatile uint16_t value[NUM_ADCS];
	Time scan_period;
} ADCState;


static ADCState g_theADC;

static void init_adc(ADCState *adc, Time scan_period);
static void adc_update(ADCState *adc);
static uint16_t read_adc_raw(uint8_t adc_channel);



/////////////// HAL (public interface) functions ////////////////


void hal_init_adc(Time scan_period)
{
	init_adc(&g_theADC, scan_period);
}


void hal_init_adc_channel(uint8_t idx)
{
	assert((1<<idx) & AVAILABLE_ADCS);
#ifndef RAW_ADC
	g_theADC.enable[idx] = TRUE;
#endif
	switch (idx)
	{
		case 0: gpio_make_input_no_pullup(GPIO_C0); break;
		case 1: gpio_make_input_no_pullup(GPIO_C1); break;
		case 2: gpio_make_input_no_pullup(GPIO_C2); break;
		case 3: gpio_make_input_no_pullup(GPIO_C3); break;
		case 4: gpio_make_input_no_pullup(GPIO_C4); break;
		case 5: gpio_make_input_no_pullup(GPIO_C5); break;
	}
}



uint16_t hal_read_adc(uint8_t idx)
{
#ifdef RAW_ADC
	return read_adc_raw(idx);
#endif

	uint16_t value;
	uint8_t old_interrupts = hal_start_atomic();
	value = g_theADC.value[idx];
	hal_end_atomic(old_interrupts);
	return value;
}



///////// Hardware Interfacing //////////////////////////


static void init_adc(ADCState *adc, Time scan_period)
{
	adc->func = (ActivationFunc) adc_update;
	adc->scan_period = scan_period;
	uint8_t idx;
	for (idx=0; idx<NUM_ADCS; idx++)
	{
		adc->enable[idx] = FALSE;
	}

	reg_set(&ADCSRA, ADEN); // enable ADC

	schedule_us(1, (Activation*) adc);
}


static void adc_update(ADCState *adc)
{
	schedule_us(adc->scan_period, (Activation *) adc);

	uint8_t adc_channel;

	for (adc_channel = 0; adc_channel < NUM_ADCS; adc_channel++) {
		if (!adc->enable[adc_channel]) { continue; }

		uint16_t newval = read_adc_raw(adc_channel);
		adc->value[adc_channel] = 
			(6*adc->value[adc_channel] + 4*newval) / 10;
			// Exponentially-weighted moving average
	}
}

static void adc_busy_wait_conversion()
{
	// start conversion
	reg_set(&ADCSRA, ADSC);
	// wait for conversion to complete
	while (reg_is_set(&ADCSRA, ADSC))
		{}
}

static uint16_t read_adc_raw(uint8_t adc_channel)
{
	ADMUX = adc_channel; // select adc channel

	adc_busy_wait_conversion();

	// Turns out the rules on when you can set ADMUX and expect it to
	// affect the next sample are a little confusing. (ATmega8 page 200).
	// I didn't think we were actually violating any of them, but
	// apparently we were, because busy-delaying after setting ADMUX
	// and before taking the sample cured the crosstalk problem.
	// For now, I work around by just taking another sample (sloooow)
	// to be sure we have a legitimate one.

	adc_busy_wait_conversion();

	/*
	ATmega8 page 198:
	ADCL must be read first, then ADCH, to ensure that the content of the Data
	Registers belongs to the same conversion.
	*/
	uint16_t newval = ADCL;
	newval |= ((uint16_t) ADCH << 8);

	return newval;
}
