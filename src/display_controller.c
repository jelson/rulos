#include "display_controller.h"
#include "util.h"

// offset 32
uint8_t sevseg_ascii[] = {
	0b0,		// space
	0b10110000,	//!
	0b0100010,	//"
	0b1100011,	//# (UGLY)
	SEVSEG_S,	//$ (UGLY)
	0b0100010,	//% (UGLY)
	SEVSEG_6,	//& (UGLY)
	0b0100000,	//'
	SEVSEG_C,	//(
	0b1111000,	//)
	SEVSEG_H,	//* (UGLY)
	0b0110001,	//+ (UGLY)
	SEVSEG_COMMA,
	SEVSEG_HYPHEN,
	0b10000000,	//.
	0b0100101,	// /
	SEVSEG_0,
	SEVSEG_1,
	SEVSEG_2,
	SEVSEG_3,
	SEVSEG_4,
	SEVSEG_5,
	SEVSEG_6,
	SEVSEG_7,
	SEVSEG_8,
	SEVSEG_9,
	0b0010010,	// : (UGLY)
	0b10010010,	// ; (UGLY)
	0b1000011,	// < (UGLY)
	0b0001001,	// =
	0b1100001,	// > (UGLY)
	0b1100101,	// ?
	0b1111101,	// @
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
	SEVSEG_Z,
	SEVSEG_C,	// [
	0b0010011,	// \ (no, really, compiler -- just a backslash)
	0b1111000,	// ]
	0b1100010,	// ^ (UGLY)
	0b0001000,	// _
	0b1100000,	// `
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
	SEVSEG_Z,
	0b0110001,	// \{ (UGLY)
	0b0000110,	// | (subtle)
	0b0000111,	// \} (UGLY)
	0b1000000,	// ~
};

uint8_t *sevseg_digits = &sevseg_ascii['0'];
uint8_t *sevseg_letters = &sevseg_ascii['A'];

void program_cell(uint8_t board, uint8_t digit, SSBitmap bitmap)
{
	int segment; 	/* must be signed! */
	uint8_t shape = (uint8_t) bitmap;

	for (segment = 6; segment >= 0; segment--)
	{
		program_segment(board, digit, segment, shape & 0x1);
		shape >>= 1;
	}

	/* consider the high bit to be the decimal */
	program_segment(board, digit, 7, shape);
}

void program_board(uint8_t board, SSBitmap *bitmap)
{
	int i;
	for (i=0; i<NUM_DIGITS; i++)
	{
		program_cell(board, i, bitmap[i]);
	}
}

#if 0
void program_string(uint8_t board, char *string)
{
	int i;
	for (i = 0; i < NUM_DIGITS; i++)
	{
		if (*string==0) { break; }
		SSBitmap symbol = ascii_to_bitmap(*string);
		if (string[1] == '.')
		{
			symbol |= SSB_DECIMAL;
			string += 2;
		}
		else
		{
			string++;
		}
		program_cell(board, NUM_DIGITS - 1 - i, symbol);
	}
}
#endif

int int_to_string(char *strp, uint8_t min_width, int zero_padded, uint32_t i)
{
	int c = 0;
	int neg = 0;
	char *ptr;
	
	if (strp!=0)
	{
		int ct = int_to_string(0, min_width, zero_padded, i);
		ptr = strp+ct-1;
		ptr[1] = '\0';
	}

	if (i < 0)
	{
		i = -i;
		neg = 1;
	}
	if (i==0)
	{
		if (strp!=0) {
			ptr[-c] = '0';
		}
		c+=1;
	}
	else
	{
		while (i>0)
		{
			if (strp!=0) {
				ptr[-c] = '0' + i%10;
			}
			c+=1;
			i /= 10;
		}
	}
	if (neg)
	{
		if (strp!=0) {
			ptr[-c] = '-';
		}
		c+=1;
	}
	while (c<min_width)
	{
		if (strp!=0) {
			ptr[-c] = zero_padded ? '0' : ' ';
		}
		c+=1;
	}
	//LOGF((logfp, "ct %d c0 %02x str %s\n", c, strp?strp[0]:"_", strp));
	return c;
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
	if (a<' ' || a>=127)
	{
		return 0;
	}
	return sevseg_ascii[((uint8_t)a)-32];
}

void ascii_to_bitmap_str(SSBitmap *b, char *a)
{
	int i;
	for (i=0; i<NUM_DIGITS && a[i]!='\0'; i++)
	{
		b[NUM_DIGITS-1-i] = ascii_to_bitmap(a[i]);
	}
}
