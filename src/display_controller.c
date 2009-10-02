#include "display_controller.h"
#include "util.h"

// offset 32
uint8_t sevseg_ascii[] = {
#include "sevseg_bitmaps.ch"
};

uint8_t *sevseg_digits = &sevseg_ascii['0'-32];
uint8_t *sevseg_letters = &sevseg_ascii['A'-32];

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

int int_to_string(char *strp, uint8_t min_width, int zero_padded, uint32_t i)
{
	int c = 0;
	int neg = 0;
	char *ptr = 0;
	
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

void ascii_to_bitmap_str(SSBitmap *b, int max_len, char *a)
{
	int i;
	for (i=0; i<max_len && a[i]!='\0'; i++)
	{
		b[i] = ascii_to_bitmap(a[i]);
	}
}
