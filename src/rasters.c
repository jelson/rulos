#include "rocket.h"
#include "rasters.h"
#include "util.h"

void raster_big_digit_update(RasterBigDigit *digit);

#include "rasters_auto.ch"

uint16_t get_bitfield(uint8_t *bytes, uint16_t offset, uint8_t len)
{
	uint8_t bitsAvaiableThisByte = 8-(offset%8);
	uint8_t bitsUsedThisByte = min(bitsAvaiableThisByte, len);
	uint16_t v = bytes[offset / 8] >> (bitsAvaiableThisByte - bitsUsedThisByte);
	v &= ((1<<bitsUsedThisByte)-1);
	uint16_t outVal = (v << (len-bitsUsedThisByte));

	uint8_t rest_len = len - bitsUsedThisByte;
	uint16_t rest_offset = offset + bitsUsedThisByte;
	if (rest_len > 0)
	{
		assert(rest_offset > offset);
		assert((rest_offset % 8)==0);
		outVal |= get_bitfield(bytes, rest_offset, rest_len);
	}
	return outVal;
}

void raster_draw_sym(RectRegion *rrect, char sym)
{
	RasterIndex *ri = rasterIndex;
	while (ri->sym != '\0' && ri->sym != sym)
	{
		ri++;
	}
	if (ri->sym=='\0')
	{
		assert(FALSE);	// symbol not found
		return;
	}
	
	int idx;
	int y=0;
	for (idx=0; idx<ri->len; idx++)
	{
		uint16_t bitfield = get_bitfield(rasterData, ri->offset_bits+idx*11, 11);
		uint8_t ybit = (bitfield >> 10) & 1;
		uint8_t x0 = (bitfield >> 5) & 0x1f;
		uint8_t x1 = (bitfield >> 0) & 0x1f;
		if (ybit) { y+=1; }
		//LOGF((logfp, "raster_draw_sym(%c idx %2d/%2d) y%d x0 %2d x1 %2d\n", sym, idx, ri->len, y, x0, x1));
		int x;
		for (x=x0; x<x1; x++)
		{
			//LOGF((logfp, "paintpixel(%d,%d)\n", x, y));
			raster_paint_pixel(rrect, x, y);
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
	int maj_x = x/4;
	int min_x = x-(maj_x*4);
	int maj_y = y/6;
	int min_y = y-(maj_y*6);

	//LOGF((logfp, "x-painted y%2d.%d x%2d.%d\n", maj_y, min_y, maj_x, min_x));
	if (maj_y<0 || maj_y >= rrect->ylen
		|| maj_x<rrect->x || maj_x >= rrect->x+rrect->xlen)
	{
		//LOGF((logfp, "discard: %d %d ; %d %d\n", 0, rrect->ylen, rrect->x, rrect->x+rrect->xlen));
		return;
	}

	//LOGF((logfp, "PAINT!\n"));
	rrect->bbuf[maj_y]->buffer[maj_x] |= _sevseg_pixel_mask[min_y][min_x];
}

void raster_big_digit_init(RasterBigDigit *digit, uint8_t board0)
{
	digit->func = (ActivationFunc) raster_big_digit_update;
	int r;
	for (r=0; r<4; r++)
	{
		board_buffer_init(&digit->bbuf[r]);
		board_buffer_push(&digit->bbuf[r], board0+r);
		digit->bbufp[r] = &digit->bbuf[r];
	}
	digit->rrect.bbuf = digit->bbufp;
	digit->rrect.ylen = 4;
	digit->rrect.x = 0;
	digit->rrect.xlen = 8;
	digit->startTime = clock_time();

	schedule(1, (Activation*) digit);
}

void raster_big_digit_update(RasterBigDigit *digit)
{
	raster_clear_buffers(&digit->rrect);

	schedule(1000, (Activation*) digit);
	Time t = clock_time() - digit->startTime;
	int s = 9 - ((t / 1000) % 10);
	raster_draw_sym(&digit->rrect, '0'+s);

	raster_draw_buffers(&digit->rrect);
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
