#ifndef _region_h
#define _region_h

#include <inttypes.h>

#include "board_buffer.h"

typedef struct {
	BoardBuffer *bbuf;
	uint8_t x;
	uint8_t xlen;
} RowRegion;

typedef struct {
	BoardBuffer **bbuf;
	uint8_t ylen;
	uint8_t x;
	uint8_t xlen;
} RectRegion;

void region_hide(RectRegion *rr);
void region_show(RectRegion *rr, int board0);

#endif // _region_h
