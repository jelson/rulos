/*
 * hardware.c: These functions are only needed for physical display hardware.
 *
 * This file is not compiled by the simulator.
 */
#define V1PCB

#include <avr/boot.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "rocket.h"
#include "hardware.h"
#include "uart.h"

#if defined(PROTO_BOARD)
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
#elif defined(V1PCB)
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
#endif


typedef struct {
	ActivationFunc f;

	/* keypad */
	char keypad_q[20];
	char keypad_last;
	uint32_t keypad_next_allowed_key_time;
} InputBufferAct_t;

static void scan_inputs(InputBufferAct_t *iba);

/*
 * Global instance of the input scanner state struct
 */
static InputBufferAct_t iba = {
	.f = (ActivationFunc) scan_inputs,
	.keypad_last = 0,
	.keypad_next_allowed_key_time = 0
};
	 

static void init_pins()
{
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

	gpio_make_input(KEYPAD_COL0);
	gpio_make_input(KEYPAD_COL1);
	gpio_make_input(KEYPAD_COL2);
	gpio_make_input(KEYPAD_COL3);
}


/*
 * 0x1738 - 0x16ca new
 * 0x1708 - 0x16c8 old
 */
void hal_program_segment(uint8_t board, uint8_t digit, uint8_t segment, uint8_t onoff)
{
	uint8_t rdigit = NUM_DIGITS - 1 - digit;

	gpio_set_or_clr(BOARDSEL0, board & (1 << 0));
	gpio_set_or_clr(BOARDSEL1, board & (1 << 1));
	gpio_set_or_clr(BOARDSEL2, board & (1 << 2));

	gpio_set_or_clr(DIGSEL0, rdigit & (1 << 0));	
	gpio_set_or_clr(DIGSEL1, rdigit & (1 << 1));	
	gpio_set_or_clr(DIGSEL2, rdigit & (1 << 2));	
	
	gpio_set_or_clr(SEGSEL0, segment & (1 << 0));
	gpio_set_or_clr(SEGSEL1, segment & (1 << 1));
	gpio_set_or_clr(SEGSEL2, segment & (1 << 2));

	/* sense reversed because cathode is common */
	gpio_set_or_clr(DATA, !onoff);

	gpio_clr(STROBE);
	_NOP();
	gpio_set(STROBE);
}


Handler timer_handler;

ISR(TIMER1_COMPA_vect)
{
	timer_handler();
}

/* clock calibration by jelson: took 209 seconds to lose one second */
uint32_t f_cpu[] = {
	(1000000 * 209 / 210),
	(2000000 * 209 / 210),
	(4000000 * 209 / 210),
	(8000000 * 209 / 210),
	};

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
void hal_start_clock_us(uint32_t us, Handler handler)
{
	uint8_t cksel = boot_lock_fuse_bits_get(GET_LOW_FUSE_BITS) & 0x0f;

	uint32_t target;
	uint8_t tccr1b = _BV(WGM12);		// CTC Mode 4 (interrupt on count-up)
	if (us < 1000)
	{
		tccr1b |= _BV(CS10);				// Prescaler = 1
		target = ((f_cpu[cksel-1] / 100) * us) / 10000;
	}
	else
	{
		tccr1b |= _BV(CS11) | _BV(CS10);	// Prescaler = 64
		target = (f_cpu[cksel-1] / 6400) * us / 10000;
	}

	timer_handler = handler;
	cli();

	OCR1A = (unsigned int) target;
	TCCR1A = 0;
	TCCR1B = tccr1b;

	/* enable output-compare int. */
	TIMSK |= (1<<OCIE1A);

	/* reset counter */
	TCNT1 = 0; 

	/* re-enable interrupts */
	sei();
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
// Ex: +20 makes the clock tick 2% faster.
// 
void hal_speedup_clock_ppm(uint32_t ratio)
{
	uint32_t adjustment = OCR1A;
	adjustment *= -ratio;
	adjustment /= 1000000;
	OCR1A += (uint16_t) adjustment;
}


/**************************************************************/
/*			 Interrupt-driven senor input                     */
/**************************************************************/
// jonh hardcodes this for a specific interrupt line. Not sure
// yet how to generalize; will do when needed, I guess.

Handler sensor_interrupt_handler;

void sensor_interrupt_register_handler(Handler handler)
{
	sensor_interrupt_handler = handler;
}

ISR(INT0_vect)
{
	sensor_interrupt_handler();
}

/**************************************************************/
/*			 Keyboard input									  */
/**************************************************************/

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


static char scan_keypad()
{
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

	return 0;
}

#define KEY_SCAN_INTERVAL_US 10000
#define KEY_REFRACTORY_TIME_US 30000

/*
 * This is called periodically to scan the keypad, debounce the keys,
 * and enqueue any characters that survive.
 */
static void debounce_keypad(InputBufferAct_t *iba)
{
	char k = scan_keypad();

	// if the same key is down, or up, ignore it
	if (k == iba->keypad_last)
		return;

	iba->keypad_last = k;

	// key was just released.  Set refactory time.
	if (k == 0) {
		iba->keypad_next_allowed_key_time = clock_time_us() + KEY_REFRACTORY_TIME_US;
		return;
	}

	// key was just pushed.  if refrac time has arrived, queue it
	if (clock_time_us() >= iba->keypad_next_allowed_key_time) {
		ByteQueue_append((ByteQueue *)iba->keypad_q, k);
	}
	
}

static void scan_inputs(InputBufferAct_t *iba)
{
	schedule_us(KEY_SCAN_INTERVAL_US, (Activation *) iba);
	debounce_keypad(iba);
}


char hal_read_keybuf()
{
	uint8_t k;

	if (ByteQueue_pop((ByteQueue *) iba.keypad_q, &k))
		return k;
	else
		return 0;
}

//// uart stuff

ISR(USART_RXC_vect)
{
	uart_receive(UDR);
}

void hal_uart_init(uint16_t baud)
{
	// disable interrupts
	cli();

	// set baud rate
	UBRRH = (unsigned char) baud >> 8;
	UBRRL = (unsigned char) baud;

	// enable receiver and receiver interrupts
	UCSRB = _BV(RXEN) | _BV(RXCIE);

	// set frame format: async, 8 bit data, 1 stop  bit, no parity
	UCSRC = _BV(UCSZ0) | _BV(UCSZ1);

	// enable interrupts
	sei();
}

/*************************************************************************************/

void hal_start_atomic()
{
	cli();
}

void hal_end_atomic()
{
	sei();
}

void hal_idle()
{
	// just busy-wait on microcontroller.
}

void hal_init()
{
	init_pins();

	ByteQueue_init((ByteQueue *) iba.keypad_q, sizeof(iba.keypad_q));
	schedule_us(1, (Activation *) &iba);
}
