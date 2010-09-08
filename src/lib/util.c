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

#include "rocket.h"

#if SIM

FILE *logfp = NULL;

void util_init()
{
	logfp = fopen("log", "w");
}
#else //!SIM
void util_init()
{
}
#endif

int32_t bound(int32_t v, int32_t l, int32_t h)
{
	if (v<l) { return l; }
	if (v>h) { return h; }
	return v;
}

uint32_t isqrt(uint32_t v)
{
	// http://www.embedded.com/98/9802fe2.htm, listing 2
	uint32_t square = 1;
	uint32_t delta = 3;
	while (square < v)
	{
		square += delta;
		delta += 2;
	}
	return (delta/2-1);
}

int int_div_with_correct_truncation(int a, int b)
{
	if (b<0) { a=-a; b=-b; }	// b is positive
	if (a>0)
	{
		return a/b;
	}
	else
	{
		int tmp = (-a)/b;
		if (tmp*b < -a)
		{
			// got truncated; round *up*
			tmp += 1;
		}
		return -tmp;
	}
}

char hexmap[16] = "0123456789ABCDEF";

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

uint16_t stack_ptr()
{
	uint8_t a, *b=&a;
	return (unsigned) b;
}

void debug_delay(int ms)
{
	volatile int i, j;
	for (i=0; i<ms; i++)
	{
		for (j=0; j<1000; j++)
		{
		}
	}
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


