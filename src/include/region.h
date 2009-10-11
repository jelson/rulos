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

#endif // _region_h
