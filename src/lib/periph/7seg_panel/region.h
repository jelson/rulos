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

#pragma once

#include <inttypes.h>

#include "periph/7seg_panel/board_buffer.h"

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
