#ifndef _laserfont_h
#define _laserfont_h

#include <inttypes.h>
#include "rocket.h"

#define LASERFONT_LOWEST_CHAR		(32)
#define LASERFONT_HIGHEST_CHAR	(126)

typedef struct {
	uint8_t widthpairs[(LASERFONT_HIGHEST_CHAR - LASERFONT_LOWEST_CHAR)/2+1];
	SSBitmap data[];
} LaserFont;

uint8_t laserfont_draw_char(LaserFont *lf, SSBitmap *bm, int size, char c);
int laserfont_draw_string(LaserFont *lf, SSBitmap *bm, int size, char *s);

#endif // _laserfont_h
