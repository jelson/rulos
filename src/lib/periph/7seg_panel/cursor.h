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

#include "periph/7seg_panel/board_buffer.h"
#include "periph/7seg_panel/region.h"

#define MAX_HEIGHT 5

// Sentinel value used in cursor_set_label.
extern char cursor_label_white[1];

typedef struct s_cursor_act {
	BoardBuffer bbuf[MAX_HEIGHT];
	uint8_t visible;
	RectRegion rr;
	uint8_t alpha;
	const char *label;
} CursorAct;

// Initialize an unassigned cursor. 
void cursor_init(CursorAct *act);

// Configure the cursor to cover the region rr, which may span multiple boards.
void cursor_show(CursorAct *act, RectRegion rr);

// Hide the cursor.
void cursor_hide(CursorAct *act);

// Configure the appearance of the cursor when shown.
// If CursorAct->label points to the sentinel cursor_label_white, the cursor
// blinks as a white box over the underlying text.
// If CursorAct->label == NULL, the cursor has a rectangular border, but is
// filled with white (no text).
// Otherwise, CursorAct->label points at text that is drawn inside a
// rectangular border.
void cursor_set_label(CursorAct *act, const char *label);
