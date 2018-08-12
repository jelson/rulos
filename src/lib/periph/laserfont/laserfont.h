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

#ifndef _laserfont_h
#define _laserfont_h

#include <inttypes.h>
#include "rocket.h"

#define LASERFONT_LOWEST_CHAR		(32)
#define LASERFONT_HIGHEST_CHAR	(126)

typedef struct {
	uint8_t widthpairs[(LASERFONT_HIGHEST_CHAR - LASERFONT_LOWEST_CHAR)/2+1];
	SSBitmap data;
} LaserFont;

uint8_t laserfont_draw_char(LaserFont *lf, SSBitmap *bm, int size, char c);
int laserfont_draw_string(LaserFont *lf, SSBitmap *bm, int size, char *s);

#endif // _laserfont_h
