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

#include "pov.h"

typedef struct {
	uint8_t width;
	uint8_t bitmaps[5];
} Glyph;
Glyph alphabet[] = {
	{5,  {0b11000, 0b01010, 0b01001, 0b01010, 0b11000,	}}, // A
	{4,  {0b11111, 0b10101, 0b10101, 0b01010, 			}}, // B
	{4,  {0b01110, 0b10001, 0b10001, 0b10001,			}}, // C
	{4,  {0b11111, 0b10001, 0b10001, 0b01110,			}}, // D
	{4,  {0b11111, 0b10101, 0b10101, 0b10001,			}}, // E
	{4,  {0b11111, 0b00101, 0b00101, 0b00001,			}}, // F
	{5,  {0b01110, 0b10001, 0b10001, 0b10001, 0b11100,	}}, // G
	{4,  {0b11111, 0b00100, 0b00100, 0b11111,			}}, // H
	{3,  {0b10001, 0b11111, 0b10001,					}}, // I
	{4,  {0b01000, 0b10001, 0b11111, 0b00001,			}}, // J
	{4,  {0b11111, 0b00100, 0b01010, 0b10001,			}}, // K
	{3,  {0b11111, 0b10000, 0b10000,					}}, // L
	{5,  {0b11111, 0b00010, 0b00100, 0b00010, 0b11111, 	}}, // M
	{5,  {0b11111, 0b00010, 0b00100, 0b01000, 0b11111,	}}, // N
	{4,  {0b01110, 0b10001, 0b10001, 0b01110,			}}, // O
	{4,  {0b11111, 0b00101, 0b00101, 0b00010,			}}, // P
	{5,  {0b01110, 0b10001, 0b10001, 0b01110, 0b10000,	}}, // Q
	{5,  {0b11111, 0b00101, 0b00101, 0b01010, 0b10000,	}}, // R
	{4,  {0b10010, 0b10101, 0b10101, 0b01001,			}}, // S
	{5,  {0b00001, 0b00001, 0b11111, 0b00001, 0b00001,	}}, // T
	{4,  {0b01111, 0b10000, 0b10000, 0b11111,			}}, // U
	{5,  {0b00011, 0b01100, 0b10000, 0b01100, 0b00011,	}}, // V
	{5,  {0b11111, 0b01000, 0b00110, 0b01000, 0b11111,	}}, // W
	{5,  {0b10001, 0b01010, 0b00100, 0b01010, 0b10001,	}}, // X
	{5,  {0b00001, 0b00010, 0b11100, 0b00010, 0b00001,	}}, // Y
	{5,  {0b10001, 0b11001, 0b10101, 0b10011, 0b10001,	}}, // Z
};
	
/*
	LED mapping:
	LED A : PROG 1 : PB4
	 nc   : PROG 2 : Vcc
	LED B : PROG 3 : PB5
	LED C : PROG 4 : PB3
	LED D : PROG 5 : PC6 (Reset)
	 gnd  : PROG 6 : Gnd
	 nc   : cabl 7 : nc
	LED E : cabl 8 : PD6
*/

void pov_init(PovAct *povAct)
{

	povAct->lastPhase = 0;
	povAct->curPhase = 0;
	povAct->lastPeriod = 1;
	povAct->visible = FALSE;

	gpio_make_output(POVLEDA);
	gpio_make_output(POVLEDB);
	gpio_make_output(POVLEDC);
	gpio_make_output(POVLEDD);
	gpio_make_output(POVLEDE);

	pov_write(povAct, "HELLO WORLD");
	
	schedule_us(1, (ActivationFuncPtr) pov_display, povAct);
}

void pov_measure(PovAct *povAct)
{
	povAct->lastPhase = povAct->curPhase;
	povAct->curPhase = clock_time_us();
	Time period = povAct->curPhase - povAct->lastPhase;
	if (period==0)
	{
		period = 1;
	}
	povAct->lastPeriod = period;
}

void pov_paint(uint8_t bitmap)
{
	gpio_set_or_clr(POVLEDA, (bitmap & 0x01));
	gpio_set_or_clr(POVLEDB, (bitmap & 0x02));
	gpio_set_or_clr(POVLEDC, (bitmap & 0x04));
	gpio_set_or_clr(POVLEDD, (bitmap & 0x08));
	gpio_set_or_clr(POVLEDE, (bitmap & 0x10));
}

void pov_display(PovAct *povAct)
{
	uint16_t position = ((clock_time_us() - povAct->curPhase) << (POV_LG_DISPLAY_WIDTH+3)) / povAct->lastPeriod;

	// shift phase 90 degrees to center fwd/reverse images
#define NINETY_DEGREES (1<<(POV_LG_DISPLAY_WIDTH-2))
	if (position < NINETY_DEGREES)
	{
		position += 3*NINETY_DEGREES;
	}
	else
	{
		position -= NINETY_DEGREES;
	}
	
	r_bool display_on = (position & (1<<(POV_LG_DISPLAY_WIDTH+1))) > 0;
	r_bool reverse = (position & (1<<(POV_LG_DISPLAY_WIDTH+2))) == 0;
	uint16_t column = (position>>1) & ((1<<POV_LG_DISPLAY_WIDTH)-1);

	uint8_t bitmap = 0;
	if (display_on)
	{
		if (reverse)
		{
			column = ~column;
		}
		bitmap = povAct->message[column & ((1<<POV_LG_DISPLAY_WIDTH)-1)];
	}
	if (povAct->visible)
	{
		pov_paint(bitmap);
	}

	schedule_us(1000, (ActivationFuncPtr) pov_display, povAct);
}

uint8_t pov_write_count(PovAct *povAct, char *msg, uint8_t offset)
{
	uint8_t bit_p;
	char *msg_p;
	for (bit_p=0; bit_p<POV_DISPLAY_WIDTH; bit_p++)
	{
		povAct->message[bit_p] = 0;
	}
	//memset(povAct->message, POV_DISPLAY_WIDTH, 0);
	for (msg_p = msg, bit_p=offset; *msg_p!=0 && (bit_p < POV_DISPLAY_WIDTH); msg_p++)
	{
		uint8_t column;
		if (*msg_p==' ')
		{
			bit_p += 4;
			continue;
		}
		uint8_t index = (*msg_p)-'A';
		if (index>(sizeof(alphabet)/sizeof(Glyph))-1)
		{
			// not a letter.
			continue;
		}

		Glyph *g = &alphabet[index];
		for (column = 0; column<g->width && (bit_p < POV_DISPLAY_WIDTH); column++)
		{
			povAct->message[bit_p] = g->bitmaps[column];
			bit_p += 1;
		}
		bit_p += 2;
	}

	if (bit_p>=2)
	{
		bit_p -= 2;
	}
	return bit_p;
}

void pov_write(PovAct *povAct, char *msg)
{
	uint8_t width = pov_write_count(povAct, msg, 0);
	pov_write_count(povAct, msg, (POV_DISPLAY_WIDTH - width)>>1);
}

void pov_set_visible(PovAct *povAct, r_bool visible)
{
	povAct->visible = visible;
}

