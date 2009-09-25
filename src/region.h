#ifndef _region_h
#define _region_h

typedef struct {
	SSBitmap *bmp;
	uint8_t x;
	uint8_t xlen;
} RowRegion;

typedef struct {
	SSBitmap **bmp;
	uint8_t off;
	uint8_t len;
} RectRegion;

#endif // _region_h
