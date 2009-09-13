// #define GO_SLOWLY
// #define SEGMENT_TEST
// #define ALPHABET
#define IDENTIFY_DIGITS

#define F_CPU 1000000

#include <inttypes.h>

#ifdef SIM
# include <stdio.h>
#endif

#ifndef SIM
# include <avr/io.h>
# include <util/delay.h>
#endif

#include "rocket.h"

#ifdef SIM
# define _NOP() do {} while(0)
#else
# define _NOP() do { __asm__ __volatile__ ("nop"); } while (0)
#endif

/*
 * 7-segment shapes are defined as 7-bit numbers, each bit representing
 * a single segment.  From MSB to LSB (reading left to right),
 * we represent the 7 segments from A through G, like this:
 *   
 *    -a-
 *  f|   |b
 *    -g-
 *  e|   |c
 *    -d-    
 *
 * The decimal point is segment 7.
 */

#define SEVSEG_BLANK  0

#define SEVSEG_0 0b1111110
#define SEVSEG_1 0b0110000
#define SEVSEG_2 0b1101101
#define SEVSEG_3 0b1111001
#define SEVSEG_4 0b0110011
#define SEVSEG_5 0b1011011
#define SEVSEG_6 0b1011111
#define SEVSEG_7 0b1110010
#define SEVSEG_8 0b1111111
#define SEVSEG_9 0b1111011

#define SEVSEG_A 0b1110111
#define SEVSEG_B 0b0011111
#define SEVSEG_C 0b1001110
#define SEVSEG_D 0b0111101
#define SEVSEG_E 0b1001111
#define SEVSEG_F 0b1000111
#define SEVSEG_G 0b1111011
#define SEVSEG_H 0b0110111
#define SEVSEG_I 0b0110000
#define SEVSEG_J 0b0111100
#define SEVSEG_K 0
#define SEVSEG_L 0b0001110
#define SEVSEG_M 0
#define SEVSEG_N 0b0010101
#define SEVSEG_O 0b1111110
#define SEVSEG_P 0b1100111
#define SEVSEG_Q 0b1110011
#define SEVSEG_R 0b0000101
#define SEVSEG_S 0b1011011
#define SEVSEG_T 0b0001111
#define SEVSEG_U 0b0111110
#define SEVSEG_V 0
#define SEVSEG_W 0
#define SEVSEG_X 0
#define SEVSEG_Y 0b0111011
#define SEVSEG_Z 0


uint8_t sevseg_digits[] = {
	SEVSEG_0,
	SEVSEG_1,
	SEVSEG_2,
	SEVSEG_3,
	SEVSEG_4,
	SEVSEG_5,
	SEVSEG_6,
	SEVSEG_7,
	SEVSEG_8,
	SEVSEG_9
};

uint8_t sevseg_letters[] = 
{
	SEVSEG_A,
	SEVSEG_B,
	SEVSEG_C,
	SEVSEG_D,
	SEVSEG_E,
	SEVSEG_F,
	SEVSEG_G,
	SEVSEG_H,
	SEVSEG_I,
	SEVSEG_J,
	SEVSEG_K,
	SEVSEG_L,
	SEVSEG_M,
	SEVSEG_N,
	SEVSEG_O,
	SEVSEG_P,
	SEVSEG_Q,
	SEVSEG_R,
	SEVSEG_S,
	SEVSEG_T,
	SEVSEG_U,
	SEVSEG_V,
	SEVSEG_W,
	SEVSEG_X,
	SEVSEG_Y,
	SEVSEG_Z
};
/*
 * Program a single segment of a single digit.
 *
PB0   Unused
PB1   SegmentProgram / KeypadOut0   / PWM1
PB2   BoardSelect0   / KeypadOut1   / PWM2
PB3   BoardSelect1   / KeypadOut2
PB4   BoardSelect2   / KeypadOut3
PB5   Unused
    PB6 - ext clock?
    PB7 - ext clock?
PC0   DigitSelect0 / ADC
PC1   DigitSelect1 / ADC
PC2   DigitSelect2 / ADC
   PC3   Unused / ADC
   PC4 - Unused / reserved - ADC, two-wire serial
   PC5 - Unused / reserved - ADC, two-wire serial
   PC6 - /Reset
   PC7 - Does not exist

PD0 - KeypadIn0 / USART
PD1 - KeypadIn1 / USART
PD2 - KeypadIn2 / interrupt
PD3 - KeypadIn3 / interrupt
PD4 Data
PD5 SegmentSelect0
PD6 SegmentSelect1  / Analog Comparator +
PD7 SegmentSelect2  / Analog Comparator -
*/

#ifndef SIM

void program_segment(uint8_t board, uint8_t digit, uint8_t segment, uint8_t onoff)
{
	/* put board select on PortB[4..2]	*/
	PORTB = (PORTB & ~(0b111 << 2)) | ((board & 0b111) << 2);

	/* put digit select on PortC[2..0] */
	PORTC = (PORTC & ~(0b111)) | (digit & 0b111);

	/* put segment select on PortD[7..5] */
	PORTD = (PORTD & ~(0b111 << 5)) | ((segment & 0b111) << 5);

	/* put data on PortB[0] */
#ifdef COMMON_ANODE
	PORTB = (PORTB & (~0b1)) | onoff;
#else
	PORTB = (PORTB & (~0b1)) | !onoff;
#endif

	/* strobe segment program enable on PortB[1] */
	PORTB &= ~(0b10);
#ifdef GO_SLOWLY
	_delay_ms(500);
#endif
	_NOP();
	PORTB |= 0b10;
}
#endif


void program_digit(uint8_t board, uint8_t digit, uint8_t shape)
{
	/* must be signed! */
	int segment;

	for (segment = 6; segment >= 0; segment--)
	{
		program_segment(board, digit, segment, shape & 0x1);
		shape >>= 1;
	}
}

void program_decimal(uint8_t board, uint8_t digit, uint8_t onoff)
{
	/*
	 * The decimal point is segment number 7
	 */
	program_segment(board, digit, 7, onoff);
}


void program_row(uint8_t board, uint8_t shape, uint8_t decimal)
{
	uint8_t digit;

	for (digit = 0; digit < 8; digit++)
	{
		program_digit(board, digit, shape);
		program_decimal(board, digit, decimal);
	}
}


void program_matrix(uint8_t shape, uint8_t decimal)
{
	uint8_t board;

	for (board = 0; board < 8; board++)
		program_row(board, shape, decimal);
}





#define SET(port, pin) do { port = port | (1 << pin); } while (0)
#define CLR(port, pin) do { port = port & ~(1 << pin); } while (0)
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
					program_matrix(SEVSEG_B, 0);
					break;
				case 1:
					for (board = 0; board < 8; board++)
					{
						program_row(board, sevseg_digits[board], 1);
					}
					break;
				case 2:
					program_matrix(SEVSEG_D, 0);
					break;
				case 3:
					for (board = 0; board < 8; board++)
					{
						for (digit = 0; digit < 8; digit++)
						{
							program_digit(board, digit, sevseg_digits[digit]);
							program_decimal(board, digit, 1);
						}
					}
					break;
			}
			break;
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

	}
}


void timer_tick(model *m)
{
	char key = scan_keyboard();

	if (!key)
	{
		/* no key pressed and we're in identify mode; continue cycling digit identify sequence. */
		if (m->major_mode == MODE_IDENTIFY)
		{
			if (++(m->t) == 10)
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
	refresh_display(m);
}


int main()
{
#ifdef SIM
        init_sim();
#endif

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
		_delay_ms(500);
	}
#endif

	return 0;
}

#if 0

int xmain()
{
	DDRB = 0xff;
	DDRC = 0xff;
	DDRD = 0xff;
	int led = 0;
	for (;;){
		PORTB = PORTC = PORTD = 0xff;
		_delay_ms(1000);
		PORTB = PORTC = PORTD = 0x0;
		_delay_ms(1000);
	}

	for (;;) {
		PORTB = PORTC = PORTD = ~(1 << led);
		led += 1;
		if (led == 8)
		{
			led = 0;
		}
		_delay_ms(1000);
	}
}

#endif
