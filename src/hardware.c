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
#include "display_controller.h"
#include "hardware.h"

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


void init_pins()
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

void hal_init()
{
	init_pins();
}


/*
 * 0x1738 - 0x16ca new
 * 0x1708 - 0x16c8 old
 */
void program_segment(uint8_t board, uint8_t digit, uint8_t segment, uint8_t onoff)
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
  * Using CTC mode (mode 4) with 1/64 prescaler.
  *
  * 64 counter ticks takes 64/F_CPU seconds.  Therefore, to get an s-second
  * interrupt, we need to take one every s / (64/F_CPU) ticks.
  *
  * To avoid floating point, do computation in msec rather than seconds:
  * take an interrupt every ms / (64 / F_CPU / 1000) counter ticks, 
  * or, (ms * F_CPU) / (64 * 1000).
  */
void start_clock_ms(int ms, Handler handler)
{
	uint8_t cksel = boot_lock_fuse_bits_get(GET_LOW_FUSE_BITS) & 0x0f;

	long ticks_tmp = ms * f_cpu[cksel-1];
	ticks_tmp /= 64000;

	timer_handler = handler;
	cli();

	OCR1A = (unsigned int) ticks_tmp;

	/* CTC Mode 4 Prescaler 64 */
	TCCR1A = 0;
	TCCR1B = (1<<WGM12)|(1<<CS11)|(1<<CS10); 

	/* enable output-compare int. */
	TIMSK |= (1<<OCIE1A);

	/* reset counter */
	TCNT1 = 0; 

	/* re-enable interrupts */
	sei();
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


char hal_scan_keyboard()
{
		uint8_t row;

		/* Scan first row */
		gpio_clr(KEYPAD_ROW0);
		gpio_set(KEYPAD_ROW1);
		gpio_set(KEYPAD_ROW2);
		gpio_set(KEYPAD_ROW3);
		row = scan_row();
		if (row == 1)			return '1';
		if (row == 2)			return '2';
		if (row == 3)			return '3';
		if (row == 4)			return 'a';

		/* Scan second row */
		gpio_set(KEYPAD_ROW0);
		gpio_clr(KEYPAD_ROW1);
		gpio_set(KEYPAD_ROW2);
		gpio_set(KEYPAD_ROW3);
		row = scan_row();
		if (row == 1)			return '4';
		if (row == 2)			return '5';
		if (row == 3)			return '6';
		if (row == 4)			return 'b';

		/* Scan third row */
		gpio_set(KEYPAD_ROW0);
		gpio_set(KEYPAD_ROW1);
		gpio_clr(KEYPAD_ROW2);
		gpio_set(KEYPAD_ROW3);
		row = scan_row();
		if (row == 1)			return '7';
		if (row == 2)			return '8';
		if (row == 3)			return '9';
		if (row == 4)			return 'c';

		/* Scan fourth row */
		gpio_set(KEYPAD_ROW0);
		gpio_set(KEYPAD_ROW1);
		gpio_set(KEYPAD_ROW2);
		gpio_clr(KEYPAD_ROW3);
		row = scan_row();
		if (row == 1)			return 's';
		if (row == 2)			return '0';
		if (row == 3)			return 'p';
		if (row == 4)			return 'd';

		return 0;
}


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

