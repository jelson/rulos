#include "display_controller.h"

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

#define SEVSEG_SPACE 0
#define SEVSEG_UNDERSCORE 0b0001000
#define SEVSEG_HYPHEN 0b0000001
#define SEVSEG_PERIOD 0b0010000
#define SEVSEG_COMMA  0b0011000


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
#if defined(PROTO_BOARD)
	/* put board select on PortB[4..2]	*/
	PORTB = (PORTB & ~(0b111 << 2)) | ((board & 0b111) << 2);

	/* put digit select on PortC[2..0] */
	PORTC = (PORTC & ~(0b111)) | (digit & 0b111);

	/* put segment select on PortD[7..5] */
	PORTD = (PORTD & ~(0b111 << 5)) | ((segment & 0b111) << 5);

	/* put data on PortB[0].  Sense reversed because cathode is common. */
	PORTB = (PORTB & (~0b1)) | !onoff;

	/* strobe segment program enable on PortB[1] */
	PORTB &= ~(0b10);
	_NOP();
	PORTB |= 0b10;
#elif defined(V1PCB)
	/* put board select on PortB[4..2]	*/
	PORTB = (PORTB & ~(0b111 << 2)) | ((board & 0b111) << 2);

	/* put digit select on PortC[2..0] */
	PORTC = (PORTC & ~(0b111)) | (digit & 0b111);

	/*
	 * put segment select on PortD[7..5], and Data on Portd[4].  Sense of onoff reversed
	 * because cathode is common.
	 */
	PORTD = (PORTD & ~(0b1111 << 4)) | ((segment & 0b111) << 5) | ((!onoff) << 4);

	/* strobe segment program enable on PortB[1] */
	PORTB &= ~(0b10);
	_NOP();
	PORTB |= 0b10;
#else
# error  Board type not set!
#endif /* board type */

}
#endif /* sim */


void program_cell(uint8_t board, uint8_t digit, SSBitmap bitmap)
{
	/* must be signed! */
	int segment;
	uint8_t shape = (uint8_t) bitmap;

	for (segment = 6; segment >= 0; segment--)
	{
		program_segment(board, digit, segment, shape & 0x1);
		shape >>= 1;
	}
}

void program_string(uint8_t board, char *string)
{
	int i;
	for (i=0; i<NUM_DIGITS; i++)
	{
		if (string[i]==0) { break; }
		SSBitmap symbol = ascii_to_bitmap(string[i]);
		program_cell(board, NUM_DIGITS - 1 - i, symbol);
	}
}

void program_decimal(uint8_t board, uint8_t digit, uint8_t onoff)
{
	/*
	 * The decimal point is segment number 7
	 */
	program_segment(board, digit, 7, onoff);
}


void program_row(uint8_t board, SSBitmap bitmap)
{
	uint8_t digit;

	for (digit = 0; digit < NUM_DIGITS; digit++)
	{
		program_cell(board, digit, bitmap);
	}
}

void program_matrix(SSBitmap bitmap)
{
	uint8_t board;

	for (board = 0; board < NUM_BOARDS; board++)
		program_row(board, bitmap);
}

SSBitmap ascii_to_bitmap(char a)
{
	// NB doing this translation here because
	// (a) program memory is less scarce than RAM,
	// and (b) it's not too slow, because a good
	// compiler converts a switch into a static binary tree.
	switch (a) {
		case '0':	return SEVSEG_0;
		case '1':	return SEVSEG_1;
		case '2':	return SEVSEG_2;
		case '3':	return SEVSEG_3;
		case '4':	return SEVSEG_4;
		case '5':	return SEVSEG_5;
		case '6':	return SEVSEG_6;
		case '7':	return SEVSEG_7;
		case '8':	return SEVSEG_8;
		case '9':	return SEVSEG_9;
		case 'a': case 'A':	return SEVSEG_A;
		case 'b': case 'B':	return SEVSEG_B;
		case 'c': case 'C':	return SEVSEG_C;
		case 'd': case 'D':	return SEVSEG_D;
		case 'e': case 'E':	return SEVSEG_E;
		case 'f': case 'F':	return SEVSEG_F;
		case 'g': case 'G':	return SEVSEG_G;
		case 'h': case 'H':	return SEVSEG_H;
		case 'i': case 'I':	return SEVSEG_I;
		case 'j': case 'J':	return SEVSEG_J;
		case 'k': case 'K':	return SEVSEG_K;
		case 'l': case 'L':	return SEVSEG_L;
		case 'm': case 'M':	return SEVSEG_M;
		case 'n': case 'N':	return SEVSEG_N;
		case 'o': case 'O':	return SEVSEG_O;
		case 'p': case 'P':	return SEVSEG_P;
		case 'q': case 'Q':	return SEVSEG_Q;
		case 'r': case 'R':	return SEVSEG_R;
		case 's': case 'S':	return SEVSEG_S;
		case 't': case 'T':	return SEVSEG_T;
		case 'u': case 'U':	return SEVSEG_U;
		case 'v': case 'V':	return SEVSEG_V;
		case 'w': case 'W':	return SEVSEG_W;
		case 'x': case 'X':	return SEVSEG_X;
		case 'y': case 'Y':	return SEVSEG_Y;
		case 'z': case 'Z':	return SEVSEG_Z;
		case '-':	return SEVSEG_HYPHEN;
		case ' ':	return SEVSEG_SPACE;
		case '.':	return SEVSEG_PERIOD;
		case ',':	return SEVSEG_COMMA;
		case '_':
		default:	return SEVSEG_UNDERSCORE;
	}
}
