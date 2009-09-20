// #define GO_SLOWLY
// #define SEGMENT_TEST
// #define ALPHABET
#define IDENTIFY_DIGITS
//#define POT_TEST

#define V1PCB

#define F_CPU 1000000

#include <inttypes.h>
#include "display_controller.h"

#ifdef SIM
# include <stdio.h>
#endif

#ifndef SIM
# include <avr/io.h>
# include <util/delay.h>
#endif

#include "rocket.h"
#include "clock.h"
#include "display_rtc.h"

#ifdef SIM
# define _NOP() do {} while(0)
#else
# define _NOP() do { __asm__ __volatile__ ("nop"); } while (0)
#endif


#define SET(port, pin) do { port |= (unsigned char) (1 << pin); } while (0)
#define CLR(port, pin) do { port &= (unsigned char) ~(1 << pin); } while (0)
#define IS_SET(port, pin) ((port & (1 << pin)) == (1 << pin))
#define IS_CLR(port, pin) ((port & (1 << pin)) == 0)

#ifndef SIM

uint8_t scan_row()
{
	/*
	 * Scan PB4, PC0, PC1, and PC2.  Return 1..4 if any of them are low.
	 * Return 0 if they're all high.
	 */
	_NOP();
	if (IS_CLR(PINB, PORTB4))		return 1;
	if (IS_CLR(PINC, PORTC0))		return 2;
	if (IS_CLR(PINC, PORTC1))		return 3;
	if (IS_CLR(PINC, PORTC2))		return 4;
	return 0;
}


char scan_keyboard()
{
	/*
	 * (0 asserted on these)
	 * Row 1: PC3
	 * Row 2: PB0
	 * Row 3: PB2
	 * Row 4: PB3
	 *
	 * (inputs with pull-ups are checked)
	 * Col 1: PB4
	 * Col 2: PC0
	 * Col 3: PC1
	 * Col 4: PC2
	 */

	uint8_t row;

	/* PC3, PB0, PB2, PB3 are outputs */
	SET(DDRC, PORTC3);
	SET(DDRB, PORTB0);
	SET(DDRB, PORTB2);
	SET(DDRB, PORTB3);

	/* PB4, PC0, PC1, PC2 are inputs */
	CLR(DDRB, PORTB4); /* set as input */
	CLR(DDRC, PORTC0);
	CLR(DDRC, PORTC1);
	CLR(DDRC, PORTC2);
	SET(PORTB, PORTB4); /* set pullup resistor */
	SET(PORTC, PORTC0);
	SET(PORTC, PORTC1);
	SET(PORTC, PORTC2);

	/* Scan first row */
	CLR(PORTC, PORTC3);
	SET(PORTB, PORTB0);
	SET(PORTB, PORTB2);
	SET(PORTB, PORTB3);
	row = scan_row();
	if (row == 1)		return '1';
	if (row == 2)		return '2';
	if (row == 3)		return '3';
	if (row == 4)		return 'a';

	/* Scan second row */
	SET(PORTC, PORTC3);
	CLR(PORTB, PORTB0);
	SET(PORTB, PORTB2);
	SET(PORTB, PORTB3);
	row = scan_row();
	if (row == 1)		return '4';
	if (row == 2)		return '5';
	if (row == 3)		return '6';
	if (row == 4)		return 'b';

	/* Scan third row */
	SET(PORTC, PORTC3);
	SET(PORTB, PORTB0);
	CLR(PORTB, PORTB2);
	SET(PORTB, PORTB3);
	row = scan_row();
	if (row == 1)		return '7';
	if (row == 2)		return '8';
	if (row == 3)		return '9';
	if (row == 4)		return 'c';

	/* Scan fourth row */
	SET(PORTC, PORTC3);
	SET(PORTB, PORTB0);
	SET(PORTB, PORTB2);
	CLR(PORTB, PORTB3);
	row = scan_row();
	if (row == 1)		return 's';
	if (row == 2)		return '0';
	if (row == 3)		return 'p';
	if (row == 4)		return 'd';

	return 0;
}

#endif



/************************************************************************************/
/************************************************************************************/

#define MODE_IDENTIFY 0
#define MODE_SHOWKEY  1

typedef struct
{
	uint8_t major_mode;
	uint8_t minor_mode;
	uint8_t t;
	uint8_t button_pressed;
} model;

void refresh_display(model *m)
{
	uint8_t board, digit, display;

#ifndef SIM
	DDRB = 0xff;
	DDRC = 0xff;
	DDRD = 0xff;
#endif

	switch (m->major_mode)
	{
		case MODE_IDENTIFY:
			switch (m->minor_mode)
			{
				case 0:
					//program_matrix(ascii_to_bitmap('B'));
					break;
				case 1:
				/*
					for (board = 2; board < 8; board++)
					{
						program_row(board, ascii_to_bitmap('0'+board) | SSB_DECIMAL);
					}
					*/
					break;
				case 2:
					//program_matrix(ascii_to_bitmap('D'));
					break;
				case 3:
				/*
					for (board = 2; board < 8; board++)
					{
						for (digit = 0; digit < 8; digit++)
						{
							program_cell(board, digit, ascii_to_bitmap('0'+digit) | SSB_DECIMAL);
						}
					}
					*/
					break;
			}
			break;
#if 0
		case MODE_SHOWKEY:
			if (m->button_pressed >= '0' && m->button_pressed <= '9')
				display = sevseg_digits[m->button_pressed - '0'];
			else if (m->button_pressed >= 'a' && m->button_pressed <= 'z')
				display = sevseg_letters[m->button_pressed - 'a'];
			else
				display = SEVSEG_U;

			for (board = 0; board < 8; board++)
				for (digit = 0; digit < 8; digit++)
					if (digit == 0)
						program_digit(board, digit, display);
					else
						program_digit(board, digit, sevseg_digits[board]);
					

			break;
#endif
	}
}


void timer_tick(model *m)
{
	// NB jonh notes that SIM display won't update until getch() is called
	char key = scan_keyboard();

#if 0
	if (!key)
	{
		/* no key pressed and we're in identify mode; continue cycling digit identify sequence. */
		if (m->major_mode == MODE_IDENTIFY)
		{
			if (++(m->t) == 6)
			{
				m->minor_mode = (m->minor_mode + 1) % 4;
				m->t = 0;
			}
		}
		else if (m->major_mode == MODE_SHOWKEY)
		{
			if (++(m->t) == 30) 
			{
				m->major_mode = MODE_IDENTIFY;
				m->minor_mode = 0;
				m->t = 0;
			}
		}
	} else {
		/* key pressed */
		m->major_mode = MODE_SHOWKEY;
		m->button_pressed = key;
		m->t = 0;
	}
#endif
	refresh_display(m);
}

int main()
{
#ifdef SIM
        init_sim();
#endif
	clock_init();
	//install_handler(ADC, adc_handler);
	drtc_init();

#ifdef IDENTIFY_DIGITS
	model m;
	m.major_mode = MODE_IDENTIFY;
	m.minor_mode = 0;
	m.t = 0;

	for (;;)
	{
		timer_tick(&m);
		_delay_ms(50);
	}
#endif

#if 0
#ifdef SEGMENT_TEST
	int segment = 0;
	int onoff = 0;

	for (;;)
	{
		program_segment(0, 0, segment, onoff);
		segment++;
		if (segment == 7) {
			segment = 0;
			onoff = !onoff;
		}
		_delay_ms(1000);
	}
#endif


#ifdef ONOFF_TEST
	for (;;)
	{
		program_digit(0, 0, 0xff);
		_delay_ms(1000);
		program_digit(0, 0, 0);
		_delay_ms(1000);
	}
#endif

#ifdef ALPHABET
	int i = 0;
	int last_shape = 0;

	for (;;)
	{
		int shape;

		if (i < 10)
			shape = sevseg_digits[i];
		else
			shape = sevseg_letters[i-10];

		program_digit(0, 1, shape);
		program_digit(0, 0, last_shape);
		last_shape = shape;
		_delay_ms(250);
		if (++i == 36)
		{
			i = 0;
		}
	}
#endif

#ifdef IDENTIFY_DIGITS
	uint8_t board, digit;

	for (;;)
	{
		program_matrix(SEVSEG_B, 0);
		_delay_ms(500);


		for (board = 0; board < 8; board++)
		{
			program_row(board, sevseg_digits[board], 1);
		}
		_delay_ms(500);

		program_matrix(SEVSEG_D, 0);
		_delay_ms(500);

		for (board = 0; board < 8; board++)
		{
			for (digit = 0; digit < 8; digit++)
			{
				program_digit(board, digit, sevseg_digits[digit]);
				program_decimal(board, digit, 1);
			}
		}
		_delay_ms(250);
	}
#endif

#endif //0

	return 0;
}

