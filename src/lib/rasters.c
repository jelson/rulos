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
#include "rasters.h"
#include "util.h"

void raster_big_digit_update(RasterBigDigit *digit);

#include "rasters_auto.ch"

uint16_t get_bitfield(uint16_t offset, uint8_t len)
{
	uint8_t bitsAvaiableThisByte = 8-(offset%8);
	uint8_t bitsUsedThisByte = min(bitsAvaiableThisByte, len);
	uint8_t byte = pgm_read_byte(&(rasterData[offset/8]));
	uint16_t v = byte >> (bitsAvaiableThisByte - bitsUsedThisByte);
	v &= ((1<<bitsUsedThisByte)-1);
	uint16_t outVal = (v << (len-bitsUsedThisByte));

	uint8_t rest_len = len - bitsUsedThisByte;
	uint16_t rest_offset = offset + bitsUsedThisByte;
	if (rest_len > 0)
	{
		assert(rest_offset > offset);
		assert((rest_offset % 8)==0);
		outVal |= get_bitfield(rest_offset, rest_len);
	}
	return outVal;
}

void raster_draw_sym(RectRegion *rrect, char sym, int8_t dx, int8_t dy)
{
	RasterIndex *ri = rasterIndex;
	while (ri->sym != '\0' && ri->sym != sym)
	{
		ri++;
	}
	if (ri->sym=='\0')
	{
		LOGF((logfp, "sym = %d (%c)\n", sym, sym));
		assert(FALSE);	// symbol not found
		return;
	}
	
	int idx;
	int y=0;
	for (idx=0; idx<ri->len; idx++)
	{
		uint16_t bitfield = get_bitfield(ri->offset_bits+idx*11, 11);
		uint8_t ybit = (bitfield >> 10) & 1;
		uint8_t x0 = (bitfield >> 5) & 0x1f;
		uint8_t x1 = (bitfield >> 0) & 0x1f;
		if (ybit) { y+=1; }
		//LOGF((logfp, "raster_draw_sym(%c idx %2d/%2d) y%d x0 %2d x1 %2d\n", sym, idx, ri->len, y, x0, x1));
		int x;
		for (x=x0; x<x1; x++)
		{
			//LOGF((logfp, "paintpixel(%d,%d)\n", dx+x, dy+y));
			raster_paint_pixel(rrect, dx+x, dy+y);
		}
	}
}

static SSBitmap _sevseg_pixel_mask[6][4] = {
	{ 0b00000000, 0b01000000, 0b00000000, 0b00000000 },
	{ 0b00000010, 0b00000000, 0b00100000, 0b00000000 },
	{ 0b00000000, 0b00000001, 0b00000000, 0b00000000 },
	{ 0b00000100, 0b00000000, 0b00010000, 0b00000000 },
	{ 0b00000000, 0b00001000, 0b00000000, 0b10000000 },
	{ 0b00000000, 0b00000000, 0b00000000, 0b00000000 }
};

void raster_paint_pixel(RectRegion *rrect, int x, int y)
{
	raster_paint_pixel_v(rrect, x, y, TRUE);
}

void raster_paint_pixel_v(RectRegion *rrect, int x, int y, r_bool on)
{
	int maj_x = int_div_with_correct_truncation(x,4);
	int min_x = x-(maj_x*4);
	int maj_y = int_div_with_correct_truncation(y,6);
	int min_y = y-(maj_y*6);

	//LOGF((logfp, "x-painted y%2d.%d x%2d.%d\n", maj_y, min_y, maj_x, min_x));
	if (maj_y<0 || maj_y >= rrect->ylen
		|| maj_x<rrect->x || maj_x >= rrect->x+rrect->xlen)
	{
		//LOGF((logfp, "discard: %d %d ; %d %d\n", 0, rrect->ylen, rrect->x, rrect->x+rrect->xlen));
		return;
	}

	//LOGF((logfp, "PAINT!\n"));
	if (on)
	{
		rrect->bbuf[maj_y]->buffer[maj_x] |= _sevseg_pixel_mask[min_y][min_x];
	}
	else
	{
		rrect->bbuf[maj_y]->buffer[maj_x] &= ~(_sevseg_pixel_mask[min_y][min_x]);
	}
}

void raster_big_digit_init(RasterBigDigit *digit, Screen4 *s4)
{
	digit->func = (ActivationFunc) raster_big_digit_update;
	digit->startTime = clock_time_us();
	digit->s4 = s4;
	digit->focused = FALSE;

	schedule_us(1, (Activation*) digit);
}

void raster_big_digit_update(RasterBigDigit *digit)
{
	schedule_us(100000, (Activation*) digit);

	if (!digit->focused)
	{
		// this thing is EXPENSIVE! don't draw it to offscreen buffers!
		// Also, don't abuse shared s4 surface.
		return;
	}

	RectRegion *rrect = &digit->s4->rrect;
	raster_clear_buffers(rrect);

	const int spacing = 30;
	const int roll = 10;
	Time t = -(clock_time_us() - digit->startTime)/1000;
	while (t<0) { t+=1000000; }
	int tens = ((t / 10000) % 10);
	int next_tens = (tens+1)%10;
	int ones = ((t / 1000) % 10);
	int ones_offset = min((t*(spacing+roll)/1000) % (spacing+roll), spacing);
	int tens_offset = 0;
	if (ones==9)
	{
		tens_offset = ones_offset;
	}
	int next_ones = (ones+1)%10;
	raster_draw_sym(rrect, '0'+tens, 0, tens_offset);
	raster_draw_sym(rrect, '0'+next_tens, 0, -spacing+tens_offset);
	raster_draw_sym(rrect, '0'+ones, 16, ones_offset);
	raster_draw_sym(rrect, '0'+next_ones, 16, -spacing+ones_offset);

	raster_draw_buffers(rrect);
}

void raster_clear_buffers(RectRegion *rrect)
{
	int row;
	for (row=0; row<rrect->ylen; row++)
	{
		memset(rrect->bbuf[row]->buffer+rrect->x, 0, rrect->xlen);
	}
}

void raster_draw_buffers(RectRegion *rrect)
{
	int row;
	for (row=0; row<rrect->ylen; row++)
	{
		board_buffer_draw(rrect->bbuf[row]);
	}
}
