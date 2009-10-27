#ifndef _rasters_h
#define _rasters_h

#include "rocket.h"
#include "region.h"

typedef struct {
	char sym;
	uint8_t len;			// units: 11-bit fields
	uint16_t offset_bits;	// units: bits
} RasterIndex;

extern RasterIndex rasterIndex[];
extern uint8_t rasterData[];

void raster_draw_sym(RectRegion *rrect, char sym, int8_t dx, int8_t dy);
void raster_paint_pixel(RectRegion *rrect, int x, int y);
	// x in [0,32), y in [0,24)

typedef struct {
	ActivationFunc func;
	BoardBuffer bbuf[4];
	BoardBuffer *bbufp[4];
	RectRegion rrect;
	Time startTime;
} RasterBigDigit;

void raster_big_digit_init(RasterBigDigit *digit, uint8_t board0);
void raster_clear_buffers(RectRegion *rrect);
void raster_draw_buffers(RectRegion *rrect);

#endif // _rasters_h
