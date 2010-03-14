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
#include "uart.h"
#include "hal.h"

#define NUM_ADCS 6

#if defined(BOARD_PROTO)
#define BOARDSEL0	GPIO_B2
#define BOARDSEL1	GPIO_B3
#define BOARDSEL2	GPIO_B4
#define DIGSEL0		GPIO_C0
#define DIGSEL1		GPIO_C1
#define DIGSEL2		GPIO_C2
#define SEGSEL0		GPIO_D5
#define SEGSEL1		GPIO_D6
#define SEGSEL2		GPIO_D7
#define DATA		GPIO_B0
#define STROBE		GPIO_B1

#define	AVAILABLE_ADCS	0x38
#define	ASSERT_TO_BOARD	1

#elif defined(BOARD_PCB10)
#define BOARDSEL0	GPIO_B2
#define BOARDSEL1	GPIO_B3
#define BOARDSEL2	GPIO_B4
#define DIGSEL0		GPIO_C0
#define DIGSEL1		GPIO_C1
#define DIGSEL2		GPIO_C2
#define SEGSEL0		GPIO_D5
#define SEGSEL1		GPIO_D6
#define SEGSEL2		GPIO_D7
#define DATA		GPIO_D4
#define STROBE		GPIO_B1

#define KEYPAD_ROW0 GPIO_D4
#define KEYPAD_ROW1 GPIO_B2
#define KEYPAD_ROW2 GPIO_B3
#define KEYPAD_ROW3 GPIO_B4
#define KEYPAD_COL0 GPIO_D0
#define KEYPAD_COL1 GPIO_D1
#define KEYPAD_COL2 GPIO_D2
#define KEYPAD_COL3 GPIO_D3

#define	AVAILABLE_ADCS	0x38
#define	ASSERT_TO_BOARD	1

#elif defined(BOARD_PCB11)
#define BOARDSEL0	GPIO_B0
#define BOARDSEL1	GPIO_B1
#define BOARDSEL2	GPIO_B2
#define DIGSEL0		GPIO_D5
#define DIGSEL1		GPIO_D6
#define DIGSEL2		GPIO_D7
#define SEGSEL0		GPIO_B3
#define SEGSEL1		GPIO_B4
#define SEGSEL2		GPIO_B5
#define DATA		GPIO_B6
#define STROBE		GPIO_B7

#define KEYPAD_ROW0 DATA
#define KEYPAD_ROW1 BOARDSEL0
#define KEYPAD_ROW2 BOARDSEL1
#define KEYPAD_ROW3 BOARDSEL2
#define KEYPAD_COL0 GPIO_D0
#define KEYPAD_COL1 GPIO_D1
#define KEYPAD_COL2 GPIO_D2
#define KEYPAD_COL3 GPIO_D3

#define JOYSTICK_TRIGGER	GPIO_D4

#define	AVAILABLE_ADCS	0x3f
#define	ASSERT_TO_BOARD	1

#elif defined(BOARD_CUSTOM)

#define	AVAILABLE_ADCS	0x3f	// Good luck, board designer -- no safety check here
#define	ASSERT_TO_BOARD	0

#else
# error No board definition given
#endif

//////////////////////////////////////////////////////////////////////////////
// Keypad
//////////////////////////////////////////////////////////////////////////////

#define KEY_SCAN_INTERVAL_US 10000
#define KEY_REFRACTORY_TIME_US 30000

typedef struct {
	ActivationFunc func;
	char keypad_q[20];
	char keypad_last;
	Time keypad_next_allowed_key_time;
} KeypadState;

static KeypadState g_theKeypad;
static void init_keypad(KeypadState *keypad);
static void keypad_update(KeypadState *key);
static char scan_keypad();

void hal_init_keypad()
{
	init_keypad(&g_theKeypad);
}

static void init_keypad(KeypadState *keypad)
{
	keypad->func = (ActivationFunc) keypad_update;
	ByteQueue_init((ByteQueue *) keypad->keypad_q, sizeof(keypad->keypad_q));
	keypad->keypad_last = 0;
	keypad->keypad_next_allowed_key_time = clock_time_us();

	schedule_us(1, (Activation *) keypad);
}

static void keypad_update(KeypadState *key)
{
	schedule_us(KEY_SCAN_INTERVAL_US, (Activation *) key);

	char k = scan_keypad();

	// if the same key is down, or up, ignore it
	if (k == key->keypad_last)
		return;

	key->keypad_last = k;

	// key was just released.  Set refactory time.
	if (k == 0) {
		key->keypad_next_allowed_key_time = clock_time_us() + KEY_REFRACTORY_TIME_US;
		return;
	}

	// key was just pushed.  if refrac time has arrived, queue it
	if (clock_time_us() >= key->keypad_next_allowed_key_time) {
		ByteQueue_append((ByteQueue *)key->keypad_q, k);
	}
	
}

char hal_read_keybuf()
{
	uint8_t k;

	if (ByteQueue_pop((ByteQueue *) g_theKeypad.keypad_q, &k))
		return k;
	else
		return 0;
}


#ifdef KEYPAD_ROW0

static uint8_t scan_row()
{
	/*
	 * Scan the four columns in of the row.  Return 1..4 if any of
	 * them are low.  Return 0 if they're all high.
	 */
	_NOP();
	if (gpio_is_clr(KEYPAD_COL0)) return 1;
	if (gpio_is_clr(KEYPAD_COL1)) return 2;
	if (gpio_is_clr(KEYPAD_COL2)) return 3;
	if (gpio_is_clr(KEYPAD_COL3)) return 4;

	return 0;
}
#endif


static char scan_keypad()
{
#ifdef KEYPAD_ROW0
	uint8_t col;

	/* Scan first row */
	gpio_clr(KEYPAD_ROW0);
	gpio_set(KEYPAD_ROW1);
	gpio_set(KEYPAD_ROW2);
	gpio_set(KEYPAD_ROW3);
	col = scan_row();
	if (col == 1)			return '1';
	if (col == 2)			return '2';
	if (col == 3)			return '3';
	if (col == 4)			return 'a';

	/* Scan second row */
	gpio_set(KEYPAD_ROW0);
	gpio_clr(KEYPAD_ROW1);
	gpio_set(KEYPAD_ROW2);
	gpio_set(KEYPAD_ROW3);
	col = scan_row();
	if (col == 1)			return '4';
	if (col == 2)			return '5';
	if (col == 3)			return '6';
	if (col == 4)			return 'b';

	/* Scan third row */
	gpio_set(KEYPAD_ROW0);
	gpio_set(KEYPAD_ROW1);
	gpio_clr(KEYPAD_ROW2);
	gpio_set(KEYPAD_ROW3);
	col = scan_row();
	if (col == 1)			return '7';
	if (col == 2)			return '8';
	if (col == 3)			return '9';
	if (col == 4)			return 'c';

	/* Scan fourth row */
	gpio_set(KEYPAD_ROW0);
	gpio_set(KEYPAD_ROW1);
	gpio_set(KEYPAD_ROW2);
	gpio_clr(KEYPAD_ROW3);
	col = scan_row();
	if (col == 1)			return 's';
	if (col == 2)			return '0';
	if (col == 3)			return 'p';
	if (col == 4)			return 'd';
#endif

	return 0;
}


//////////////////////////////////////////////////////////////////////////////

static void init_pins()
{
#ifdef BOARDSEL0
	gpio_make_output(BOARDSEL0);
	gpio_make_output(BOARDSEL1);
	gpio_make_output(BOARDSEL2);
	gpio_make_output(DIGSEL0);
	gpio_make_output(DIGSEL1);
	gpio_make_output(DIGSEL2);
	gpio_make_output(SEGSEL0);
	gpio_make_output(SEGSEL1);
	gpio_make_output(SEGSEL2);
	gpio_make_output(DATA);
	gpio_make_output(STROBE);
#endif

#ifdef KEYPAD_COL0
	gpio_make_input(KEYPAD_COL0);
	gpio_make_input(KEYPAD_COL1);
	gpio_make_input(KEYPAD_COL2);
	gpio_make_input(KEYPAD_COL3);
#endif
}


#ifndef BOARD_CUSTOM

static uint8_t segmentRemapTables[4][8] = {
#define SRT_SUBU	0
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
#define SRT_SDBU	1
// swap decimal point with segment H, which are the nodes reversed
// when an LED is mounted upside-down
	{ 0, 1, 2, 3, 4, 5, 7, 6 },
#define SRT_SUBD	2
	{ 3, 4, 5, 0, 1, 2, 6, 7 },
#define SRT_SDBD	3
	{ 3, 4, 5, 0, 1, 2, 7, 6 },
	};
typedef uint8_t SegmentRemapIndex;

typedef struct {
	r_bool				reverseDigits;
	SegmentRemapIndex	segmentRemapIndices[8];
} BoardRemap;

static BoardRemap boardRemapTables[] = {
#define BRT_SOLDERED_UP_BOARD_UP	0
	{ FALSE, { SRT_SUBU, SRT_SUBU, SRT_SUBU, SRT_SUBU, SRT_SUBU, SRT_SUBU, SRT_SUBU, SRT_SUBU }},
#define BRT_SOLDERED_DN_BOARD_DN	1
	{ TRUE,  { SRT_SDBD, SRT_SDBD, SRT_SDBD, SRT_SDBD, SRT_SDBD, SRT_SDBD, SRT_SDBD, SRT_SDBD }},
#define BRT_WALLCLOCK				2
//	{ FALSE, { SRT_SUBU, SRT_SUBU, SRT_SDBU, SRT_SUBU, SRT_SDBU, SRT_SUBU, SRT_SUBU, SRT_SUBU }},
	{ FALSE, { SRT_SUBU, SRT_SUBU, SRT_SUBU, SRT_SDBU, SRT_SUBU, SRT_SDBU, SRT_SUBU, SRT_SUBU }},
#define BRT_CHASECLOCK				3
	{ FALSE, { SRT_SUBU, SRT_SDBU, SRT_SUBU, SRT_SUBU, SRT_SUBU, SRT_SDBU, SRT_SUBU, SRT_SUBU }},
};
typedef uint8_t BoardRemapIndex;

static BoardRemapIndex displayConfiguration[NUM_BOARDS];

uint16_t g_epb_delay_constant = 1;
void epb_delay()
{
	uint16_t delay;
	static volatile int x;
	for (delay=0; delay<g_epb_delay_constant; delay++)
	{
		x = x+1;
	}
}

/*
 * 0x1738 - 0x16ca new
 * 0x1708 - 0x16c8 old
 */
void hal_program_segment(uint8_t board, uint8_t digit, uint8_t segment, uint8_t onoff)
{
	BoardRemap *br = &boardRemapTables[displayConfiguration[board]];
	uint8_t rdigit;
	if (br->reverseDigits)
	{
		rdigit = digit;
	}
	else
	{
		rdigit = NUM_DIGITS - 1 - digit;
	}
	uint8_t asegment = segmentRemapTables[br->segmentRemapIndices[rdigit]][segment];

	gpio_set_or_clr(DIGSEL0, rdigit & (1 << 0));	
	gpio_set_or_clr(DIGSEL1, rdigit & (1 << 1));	
	gpio_set_or_clr(DIGSEL2, rdigit & (1 << 2));	
	
	gpio_set_or_clr(SEGSEL0, asegment & (1 << 0));
	gpio_set_or_clr(SEGSEL1, asegment & (1 << 1));
	gpio_set_or_clr(SEGSEL2, asegment & (1 << 2));

	gpio_set_or_clr(BOARDSEL0, board & (1 << 0));
	gpio_set_or_clr(BOARDSEL1, board & (1 << 1));
	gpio_set_or_clr(BOARDSEL2, board & (1 << 2));

	/* sense reversed because cathode is common */
	gpio_set_or_clr(DATA, !onoff);
	
	epb_delay();

	gpio_clr(STROBE);

	epb_delay();

	gpio_set(STROBE);

	epb_delay();
}

#else
void hal_program_segment(uint8_t board, uint8_t digit, uint8_t segment, uint8_t onoff)
{
	assert(FALSE);
}
#endif // BOARD_CUSTOM

void null_handler()
{
}

Handler timer1_handler = null_handler;
Handler timer2_handler = null_handler;

ISR(TIMER1_COMPA_vect)
{
	timer1_handler();
}

#if defined(MCUatmega8)
ISR(TIMER2_COMP_vect)
#elif defined (MCUatmega328p)
ISR(TIMER2_COMPA_vect)
#else
# error hardware-specific timer code needs help!
#endif
{
	timer2_handler();
}

uint32_t f_cpu_values[] = {
	(1000000),
	(2000000),
	(4000000),
	(8000000),
	};

uint32_t f_cpu;

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
	uint8_t cs;
	for (cs=1; cs<8; cs++)
	{
		// Units: 16hs = 1us
		if (timerDef->prescaler_bits[cs] == 0xff) { continue; }
		uint32_t hs_per_cpu_tick = 16000000 / f_cpu;
		uint32_t hs_per_prescale_tick = hs_per_cpu_tick << (timerDef->prescaler_bits[cs]);
		uint32_t prescale_tick_per_period =
			((req_us_per_period<<4) / hs_per_prescale_tick);
		uint32_t max_prescale_ticks = (((uint32_t) 1)<<timerDef->ocr_bits)-1;
		if (prescale_tick_per_period > max_prescale_ticks)
		{
			// go try a bigger prescaler
			continue;
		}
		*out_us_per_period = (prescale_tick_per_period * hs_per_prescale_tick) >> 4;
		*out_cs = cs;
		*out_ocr = prescale_tick_per_period;
		return;
	}
	assert(FALSE);	// might need a software scaler
}

uint32_t hal_start_clock_us(uint32_t us, Handler handler, uint8_t timer_id)
{
	uint32_t actual_us_per_period;
		// may not equal what we asked for, because of prescaler rounding.
	uint8_t cs;
	uint16_t ocr;

	uint8_t old_interrupts = hal_start_atomic();

	if (timer_id == TIMER1)
	{
		find_prescaler(us, &_timer1, &actual_us_per_period, &cs, &ocr);

		uint8_t tccr1b = _BV(WGM12);		// CTC Mode 4 (interrupt on count-up)
		tccr1b |= (cs & 4) ? _BV(CS12) : 0;
		tccr1b |= (cs & 2) ? _BV(CS11) : 0;
		tccr1b |= (cs & 1) ? _BV(CS10) : 0;

		timer1_handler = handler;

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

	/* restore interrupts */
	hal_end_atomic(old_interrupts);

	/* enable interrupts */
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


/**************************************************************/
/*			 Interrupt-driven senor input                     */
/**************************************************************/
// jonh hardcodes this for a specific interrupt line. Not sure
// yet how to generalize; will do when needed, I guess.

#if 0
Handler sensor_interrupt_handler;

void sensor_interrupt_register_handler(Handler handler)
{
	sensor_interrupt_handler = handler;
}

ISR(INT0_vect)
{
	sensor_interrupt_handler();
}
#endif

//////////////////////////////////////////////////////////////////////////////
// ADC
//////////////////////////////////////////////////////////////////////////////

typedef struct {
	ActivationFunc func;
	r_bool enable[NUM_ADCS];
	volatile uint16_t value[NUM_ADCS];
	Time scan_period;
} ADCState;

static ADCState g_theADC;

static void init_adc(ADCState *adc, Time scan_period);
static void adc_update(ADCState *adc);

void hal_init_adc(Time scan_period)
{
	init_adc(&g_theADC, scan_period);
}

// #define RAW_ADC

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

void hal_init_joystick_button()
{
#if defined(JOYSTICK_TRIGGER)
	// Only PCB11 defines a joystick trigger line
	gpio_make_input(JOYSTICK_TRIGGER);
#else
	assert(FALSE);	// no joystick on this hardware! why are you trying to read it?
#endif
}

r_bool hal_read_joystick_button()
{
#if defined(JOYSTICK_TRIGGER)
	return gpio_is_clr(JOYSTICK_TRIGGER);
#else
	return FALSE;
#endif
}

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

//////////////////////////////////////////////////////////////////////////////

//// uart stuff

#if defined(MCUatmega8)
# define _UBRRH    UBRRH
# define _UBRRL    UBRRL
# define _UCSRA    UCSRA
# define _UCSRB    UCSRB
# define _UCSRC    UCSRC
# define _RXEN     RXEN
# define _RXCIE    RXCIE
# define _UCSZ0    UCSZ0
# define _UCSZ1    UCSZ1
#elif defined(MCUatmega328p)
# define _UBRRH    UBRR0H
# define _UBRRL    UBRR0L
# define _UCSRA    UCSR0A
# define _UCSRB    UCSR0B
# define _UCSRC    UCSR0C
# define _RXEN     RXEN0
# define _RXCIE    RXCIE0
# define _UCSZ0    UCSZ00
# define _UCSZ1    UCSZ01
#else
# error Hardware-specific UART code needs love
#endif

void hal_uart_init(uint16_t baud)
{
	// disable interrupts
	uint8_t old_interrupts = hal_start_atomic();

	// set baud rate
	_UBRRH = (unsigned char) baud >> 8;
	_UBRRL = (unsigned char) baud;

	// enable receiver and receiver interrupts
	_UCSRB = _BV(_RXEN) | _BV(_RXCIE);

	// set frame format: async, 8 bit data, 1 stop  bit, no parity
	_UCSRC =  _BV(_UCSZ1) | _BV(_UCSZ0)
#ifdef MCUatmega8
	  | _BV(URSEL)
#endif
	  ;

	// enable interrupts
	hal_end_atomic(old_interrupts);
}

/*************************************************************************************/



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


uint8_t hal_start_atomic()
{
	uint8_t retval = SREG & _BV(SREG_I);
	cli();
	return retval;
}

void hal_end_atomic(uint8_t interrupt_flag)
{
	if (interrupt_flag)
		sei();
}

void hal_idle()
{
	// just busy-wait on microcontroller.
}

void hal_init(BoardConfiguration bc)
{
#ifndef BOARD_CUSTOM
	// This code is static per binary; could save some code space
	// by using #ifdefs instead of dynamic code.
	int i;
	for (i=0; i<NUM_BOARDS; i++)
	{
		displayConfiguration[i] = BRT_SOLDERED_UP_BOARD_UP;
	}
	switch (bc)
	{
		case bc_rocket0:
			displayConfiguration[0] = BRT_WALLCLOCK;
			displayConfiguration[3] = BRT_SOLDERED_DN_BOARD_DN;
			displayConfiguration[4] = BRT_SOLDERED_DN_BOARD_DN;
			break;
		case bc_rocket1:
			displayConfiguration[2] = BRT_SOLDERED_DN_BOARD_DN;
			break;
		case bc_audioboard:
			break;
		case bc_wallclock:
			displayConfiguration[0] = BRT_WALLCLOCK;
			break;
		case bc_chaseclock:
			displayConfiguration[0] = BRT_CHASECLOCK;
			break;
	}
#endif

	init_pins();

#if defined(MCUatmega8)
	// read fuses to determine clock frequency
	uint8_t cksel = boot_lock_fuse_bits_get(GET_LOW_FUSE_BITS) & 0x0f;
	f_cpu = f_cpu_values[cksel-1];
#elif defined(MCUatmega328p)
	// If we decide to do variable clock rates on 328p, see page 37
	// for prescale configurations.
	CLKPR = 0x80;
	CLKPR = 0;
	f_cpu = (uint32_t) 8000000;
#else
# error Hardware-specific clock code needs help
#endif
}


// TODO arrange for hardware to call audio_emit_sample once every 125us
// (8kHz), either by speeding up the main clock (and only firing the clock
// handler every n ticks), or by using a separate CPU clock (TIMER0)

#define AUDIO_BUF_SIZE 32
static struct s_hardware_audio {
	RingBuffer *ring;
	uint8_t _storage[sizeof(RingBuffer)+1+AUDIO_BUF_SIZE];
} audio =
	{ NULL };

void audio_write_sample(uint8_t value)
{
	// TODO send this out to the DAC
}

void audio_emit_sample()
{
	if (audio.ring==NULL)
	{
		// not initialized.
		return;
	}

	// runs in interrupt context; atomic.
	uint8_t sample = 0;
	if (ring_remove_avail(audio.ring))
	{
		sample = ring_remove(audio.ring);
	}
	audio_write_sample(sample);
}

RingBuffer *hal_audio_init(uint16_t sample_period_us)
{
	assert(sample_period_us == 125);	// not that an assert in hardware.c helps...
	audio.ring = (RingBuffer*) audio._storage;
	init_ring_buffer(audio.ring, sizeof(audio._storage));
	return audio.ring;
}

//////////////////////////////////////////////////////////////////////////////
// SPI: ATMega...32P page 82, 169
// Flash: w25x16 page 17+

HALSPIIfc *g_spi;

ISR(SPI_STC_vect)
{
	uint8_t byte = SPDR;
	g_spi->func(g_spi, byte);	// read from SPDR?
}

#define DDR_SPI	DDRB
#define DD_MOSI	DDB3
#define DD_SCK	DDB5
#define DD_SS	DDB2
#define SPI_SS	GPIO_B2
// TODO these can't be right -- GPIO_B2 is allocated to BOARDSEL2
// in V11PCB.

void hal_init_spi(HALSPIIfc *spi)
{
	g_spi = spi;
	// TODO I'm blasting the port B DDR here; we may care
	// about correct values for PB[0167].
	DDR_SPI = (1<<DD_MOSI)|(1<<DD_SCK)|(1<<DD_SS);
	SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR0);
}

void hal_spi_open()
{
	gpio_set_or_clr(SPI_SS, 0);
}

void hal_spi_send(uint8_t byte)
{
	SPDR = byte;
}

void hal_spi_close()
{
	gpio_set_or_clr(SPI_SS, 1);
}

void hardware_assert(uint16_t line)
{
#if ASSERT_TO_BOARD
	board_debug_msg(line);
#endif

	while (1) { }
}

void hardware_assign_timer_handler(uint8_t timer_id, Handler handler)
{
	if (timer_id==TIMER1)
	{
		timer1_handler = handler;
	}
	else if (timer_id==TIMER2)
	{
		timer2_handler = handler;
	}
	else
	{
		assert(FALSE);
	}
}

void debug_abuse_epb()
{
#if defined(BOARD_PCB11)
	while (TRUE)
	{
		gpio_set(BOARDSEL0);
		gpio_clr(BOARDSEL0);
		gpio_set(BOARDSEL1);
		gpio_clr(BOARDSEL1);
		gpio_set(BOARDSEL2);
		gpio_clr(BOARDSEL2);
		gpio_set(DIGSEL0);
		gpio_clr(DIGSEL0);
		gpio_set(DIGSEL1);
		gpio_clr(DIGSEL1);
		gpio_set(DIGSEL2);
		gpio_clr(DIGSEL2);
		gpio_set(SEGSEL0);
		gpio_clr(SEGSEL0);
		gpio_set(SEGSEL1);
		gpio_clr(SEGSEL1);
		gpio_set(SEGSEL2);
		gpio_clr(SEGSEL2);
		gpio_set(DATA);
		gpio_clr(DATA);
	}
#else
	assert(FALSE);
#endif // defined(BOARD_PCB11)
}

