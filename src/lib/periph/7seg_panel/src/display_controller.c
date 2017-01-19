/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

#include <string.h>

#include "display_controller.h"
#include "hal.h"

// offset 32
static const uint8_t sevseg_ascii[] PROGMEM = {
#include "lib/periph/7seg_panel/sevseg_bitmaps.ch"
};

void program_cell(uint8_t board, uint8_t digit, SSBitmap bitmap)
{
	int segment; 	/* must be signed! */
	uint8_t shape = (uint8_t) bitmap;

	for (segment = 6; segment >= 0; segment--)
	{
		hal_program_segment(board, digit, segment, shape & 0x1);
		shape >>= 1;
	}

	/* consider the high bit to be the decimal */
	hal_program_segment(board, digit, 7, shape);
}

void program_board(uint8_t board, SSBitmap *bitmap)
{
	int i;
	for (i=0; i<NUM_DIGITS; i++)
	{
		program_cell(board, i, bitmap[i]);
	}
}


void program_decimal(uint8_t board, uint8_t digit, uint8_t onoff)
{
	/*
	 * The decimal point is segment number 7
	 */
	hal_program_segment(board, digit, 7, onoff);
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
	return pgm_read_byte(&(sevseg_ascii[((uint8_t)a)-32]));
}

void ascii_to_bitmap_str(SSBitmap *b, int max_len, const char *a)
{
	int i;
	for (i=0; i<max_len && a[i]!='\0'; i++)
	{
		b[i] = ascii_to_bitmap(a[i]);
	}
}

void debug_msg_hex(uint8_t board, char *m, uint16_t hex)
{
	static char buf[9];
	static SSBitmap bm[8];

	strcpy(buf, m);
	int i;
	for (i=strlen(buf); i<8; i++)
	{
		buf[i] = ' ';
	}
	buf[8] = '\0';
	buf[4] = hexmap[(hex>>12)&0x0f];
	buf[5] = hexmap[(hex>> 8)&0x0f];
	buf[6] = hexmap[(hex>> 4)&0x0f];
	buf[7] = hexmap[(hex    )&0x0f];
	ascii_to_bitmap_str(bm, 8, buf);
	program_board(board, bm);
}

void board_debug_msg(uint16_t line)
{
	char buf[9];
	buf[0] = 'a';
	buf[1] = 's';
	buf[2] = 'r';
	int_to_string2(&buf[3], 5, 0, line);
	buf[8] = 0;
	SSBitmap bm[8];
	ascii_to_bitmap_str(bm, 8, buf);
	program_board(0, bm);
}