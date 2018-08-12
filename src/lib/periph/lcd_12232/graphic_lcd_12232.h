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

#include "core/rulos.h"

#define BITMAP_ROW_LEN	16	/* bytes; 128 pixels, six offscreen */
#define BITMAP_NUM_ROWS	32

typedef struct {
	ActivationRecord done_act;
	uint8_t framebuffer[BITMAP_NUM_ROWS][BITMAP_ROW_LEN];
} GLCD;

void glcd_init(GLCD *glcd, ActivationFuncPtr done_func, void *done_data);
//void glcd_clear_screen(GLCD *glcd);
void glcd_draw_framebuffer(GLCD *glcd);
void glcd_clear_framebuffer(GLCD *glcd);

uint8_t glcd_paint_char(GLCD *glcd, char glyph, int16_t dx0, r_bool invert);


