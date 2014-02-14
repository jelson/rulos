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

const char hexmap[] = "0123456789ABCDEF";

void debug_itoha(char *out, uint16_t i)
{
	out[0] = hexmap[(i>>(3*4)) & 0xf];
	out[1] = hexmap[(i>>(2*4)) & 0xf];
	out[2] = hexmap[(i>>(1*4)) & 0xf];
	out[3] = hexmap[(i>>(0*4)) & 0xf];
	out[4] = '\0';
}

// places 6 bytes into out.
void itoda(char *out, uint16_t v)
{
	out[5] = '\0';
	uint8_t i;
	for (i=0; i<5; i++)
	{
		uint16_t frac = v/10;
		out[4-i] = '0' + (v - frac*10);
		v = frac;
	}
}

uint16_t atoi_hex(char *in)
{
	uint16_t v = 0;
	uint8_t u;
	while (1)
	{
		char c = (*in);
		if (c>='0' && c<='9')
		{
			u = c-'0';
		}
		else if (c>='A' && c<='F')
		{
			u = c-'A'+10;
		}
		else if (c>='a' && c<='f')
		{
			u = c-'a'+10;
		}
		else
		{
			break;
		}
		v = (v<<4) | u;
		in++;
	}
	return v;
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


int int_to_string2(char *strp, uint8_t min_width, uint8_t min_zeros, int32_t i)
{
	int c = 0;
	int neg = 0;
	char *ptr = 0;
	
	//LOGF((logfp, "i %d\n", ));
	if (strp!=0)
	{
		int ct = int_to_string2(0, min_width, min_zeros, i);
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
	while (c<min_zeros)
	{
		if (strp!=0) {
			ptr[-c] = '0';
		}
		c+=1;
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
			ptr[-c] = ' ';
		}
		c+=1;
	}
	//LOGF((logfp, "  ct %d str %s\n", c, strp));
	return c;
}


#if 0
uint16_t stack_ptr()
{
	uint8_t a, *b=&a;
	return (unsigned) b;
}
#endif

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


