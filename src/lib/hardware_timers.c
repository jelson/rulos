/*
 * hardware.c: These functions are only needed for physical display hardware.
 *
 * This file is not compiled by the simulator.
 */

#include <avr/boot.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay_basic.h>

#include "rocket.h"
#include "hardware.h"
#include "board_defs.h"
#include "hal.h"



void null_handler(void *data)
{
}

Handler timer1_handler = null_handler;
void *timer1_data = NULL;

Handler timer2_handler = null_handler;
void *timer2_data = NULL;

ISR(TIMER1_COMPA_vect)
{
	timer1_handler(timer1_data);
}

#if defined(MCUatmega8)
ISR(TIMER2_COMP_vect)
#elif defined (MCUatmega328p)
ISR(TIMER2_COMPA_vect)
#else
# error hardware-specific timer code needs help!
#endif
{
	timer2_handler(timer2_data);
}

uint32_t f_cpu_values[] = {
	(1000000),
	(2000000),
	(4000000),
	(8000000),
	};

uint32_t f_cpu;


void init_f_cpu()
{
#ifdef CRYSTAL
	f_cpu = CRYSTAL;
#else
# if defined(MCUatmega8)
	// read fuses to determine clock frequency
	uint8_t cksel = boot_lock_fuse_bits_get(GET_LOW_FUSE_BITS) & 0x0f;
	f_cpu = f_cpu_values[cksel-1];
# elif defined(MCUatmega328p)
	// If we decide to do variable clock rates on 328p, see page 37
	// for prescale configurations.
	CLKPR = 0x80;
	CLKPR = 0;
	f_cpu = (uint32_t) 8000000;
# else
#  error Hardware-specific clock code needs help
# endif
#endif
}

 /* 
  * 
  * Based on http://sunge.awardspace.com/binary_clock/binary_clock.html
  *	  
  * Use timer/counter1 to generate clock ticks for events.
  *
  * For >=1ms interval,
  * Using CTC mode (mode 4) with 1/64 prescaler.
  * For smaller intervals (where a 16MHz clock won't
  * overflow a 16-bit register), we use prescaler 1.
  *
  * 64 counter ticks takes 64/F_CPU seconds.  Therefore, to get an s-second
  * interrupt, we need to take one every s / (64/F_CPU) ticks.
  *
  * To avoid floating point, do computation in usec rather than seconds:
  * take an interrupt every us / (prescaler / F_CPU / 1000000) counter ticks, 
  * or, (us * F_CPU) / (prescaler * 1000000).
  * We move around the constants in the calculations below to
  * keep the intermediate results fitting in a 32-bit int.
  */

static uint8_t _timer1_prescaler_bits[] = { 0xff, 0, 3, 6, 8, 10, 0xff, 0xff };
static uint8_t _timer2_prescaler_bits[] = { 0xff, 0, 3, 5, 6,  7,    8,   10 };
typedef struct {
	uint8_t *prescaler_bits;
	uint8_t ocr_bits;
} TimerDef;
static TimerDef _timer1 = { _timer1_prescaler_bits, 16 };
static TimerDef _timer2 = { _timer2_prescaler_bits,  8 };

static void find_prescaler(uint32_t req_us_per_period, TimerDef *timerDef,
	uint32_t *out_us_per_period,
	uint8_t *out_cs,	// prescaler selection
	uint16_t *out_ocr	// count limit
	)
{
#define HS_FACTOR 48
	uint8_t cs;
	for (cs=1; cs<8; cs++)
	{
		// Units: 48hs = 1us.  jelson changed from 16 to 48 because of my 12mhz crystal
		if (timerDef->prescaler_bits[cs] == 0xff) { continue; }
		uint32_t hs_per_cpu_tick = (HS_FACTOR*1000000) / f_cpu;
		uint32_t hs_per_prescale_tick = hs_per_cpu_tick << (timerDef->prescaler_bits[cs]);
		uint32_t prescale_tick_per_period =
			((req_us_per_period*HS_FACTOR) / hs_per_prescale_tick) + 1;
		uint32_t max_prescale_ticks = (((uint32_t) 1)<<timerDef->ocr_bits)-1;
		if (prescale_tick_per_period > max_prescale_ticks)
		{
			// go try a bigger prescaler
			continue;
		}
		*out_us_per_period = (prescale_tick_per_period * hs_per_prescale_tick) / HS_FACTOR;
		*out_cs = cs;
		*out_ocr = prescale_tick_per_period;
		return;
	}
	assert(FALSE);	// might need a software scaler
}

uint32_t hal_start_clock_us(uint32_t us, Handler handler, void *data, uint8_t timer_id)
{
	uint32_t actual_us_per_period;
		// may not equal what we asked for, because of prescaler rounding.
	uint8_t cs;
	uint16_t ocr;

	// disable interrupts
	cli();

	if (timer_id == TIMER1)
	{
		find_prescaler(us, &_timer1, &actual_us_per_period, &cs, &ocr);

		uint8_t tccr1b = _BV(WGM12);		// CTC Mode 4 (interrupt on count-up)
		tccr1b |= (cs & 4) ? _BV(CS12) : 0;
		tccr1b |= (cs & 2) ? _BV(CS11) : 0;
		tccr1b |= (cs & 1) ? _BV(CS10) : 0;

		timer1_handler = handler;
		timer1_data = data;

		OCR1A = ocr;
		TCCR1A = 0;
		TCCR1B = tccr1b;

		/* enable output-compare int. */
#if defined(MCUatmega8)
		TIMSK  |= _BV(OCIE1A);
#elif defined (MCUatmega328p)
		TIMSK1 |= _BV(OCIE1A);
#else
# error hardware-specific timer code needs help!
#endif

		/* reset counter */
		TCNT1 = 0; 
	}
	else if (timer_id == TIMER2)
	{
		find_prescaler(us, &_timer2, &actual_us_per_period, &cs, &ocr);

		uint8_t tccr2 = _BV(WGM21);		// CTC Mode 2 (interrupt on count-up)
		tccr2 |= (cs & 4) ? _BV(CS22) : 0;
		tccr2 |= (cs & 2) ? _BV(CS21) : 0;
		tccr2 |= (cs & 1) ? _BV(CS20) : 0;

		timer2_handler = handler;
		timer2_data = data;

#if defined(MCUatmega8)
		OCR2 = ocr;
		TCCR2 = tccr2;
		TIMSK  |= _BV(OCIE2); /* enable output-compare int. */
#elif defined (MCUatmega328p)
		OCR2A = ocr;
		TCCR2A = tccr2;
		TIMSK1 |= _BV(OCIE2A); /* enable output-compare int. */
#else
# error hardware-specific timer code needs help!
#endif

		/* reset counter */
		TCNT2 = 0; 
	}
	else
	{
		assert(FALSE);
	}

	/* Enable interrupts (regardless of if they were on before or not) */
	sei();

	return actual_us_per_period;
}

/*
 * How long since the last interval timer?
 * 0 == the current interval just started.
 * 1000 = the next interval is about to start.
 */
uint16_t hal_elapsed_milliintervals()
{
	return 1000 * TCNT1 / OCR1A;
}

// Speed up or slow down the clock by a certain
// ratio, expressed in parts per million
//
// A ratio of 0 will keep the clock unchanged.
// Positive ratios will make the clock tick faster.
// Negative ratios will make the clock tick slower.
// 
void hal_speedup_clock_ppm(int32_t ratio)
{
	uint8_t old_interrupts = hal_start_atomic();

	uint16_t new_ocr1a = OCR1A;
	int32_t adjustment = new_ocr1a;

	adjustment *= -ratio;
	adjustment /= 1000000;
	new_ocr1a += (uint16_t) adjustment;

	OCR1A = new_ocr1a;
	if (TCNT1 >= new_ocr1a)
		TCNT1 = new_ocr1a-1;

	hal_end_atomic(old_interrupts);
}


#if 0
void hardware_assign_timer_handler(uint8_t timer_id, Handler handler, void *data)
{
	if (timer_id==TIMER1)
	{
		timer1_handler = handler;
		timer1_data = data;
	}
	else if (timer_id==TIMER2)
	{
		timer2_handler = handler;
		timer2_data = data;
	}
	else
	{
		assert(FALSE);
	}
}
#endif

void hal_delay_ms(uint16_t __ms)
{
	// copied from util/delay.h
	uint16_t __ticks;
	double __tmp = ((f_cpu) / 4e3) * __ms;
	if (__tmp < 1.0)
		__ticks = 1;
	else if (__tmp > 65535)
        {
			//      __ticks = requested delay in 1/10 ms
			__ticks = (uint16_t) (__ms * 10.0);
			while(__ticks)
                {
					// wait 1/10 ms
					_delay_loop_2(((f_cpu) / 4e3) / 10);
					__ticks --;
                }
			return;
        }
	else
		__ticks = (uint16_t)__tmp;
	_delay_loop_2(__ticks);
}

