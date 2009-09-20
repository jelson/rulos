#include "display_controller.h"



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

void program_integer(uint8_t board, uint32_t i)
{
	int digit;
	int neg = 0;

	if (i < 0)
	{
		i = -i;
		neg = 1;
	}

	for (digit = 0; digit < NUM_DIGITS; digit++)
	{
		if (i)
		{
			program_cell(board, digit, sevseg_digits[i % 10]);
			i /= 10;
		}
		else
		{
			if (neg)
			{
				program_cell(board, digit, SEVSEG_HYPHEN);
				neg = 0;
			}
			else
			{
				program_cell(board, digit, SEVSEG_BLANK);
			}
		}
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
