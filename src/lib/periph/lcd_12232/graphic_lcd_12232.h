/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "core/rulos.h"

#define BITMAP_ROW_LEN 16 /* bytes; 128 pixels, six offscreen */
#define BITMAP_NUM_ROWS 32

typedef struct {
  ActivationRecord done_act;
  uint8_t framebuffer[BITMAP_NUM_ROWS][BITMAP_ROW_LEN];
} GLCD;

void glcd_init(GLCD *glcd, ActivationFuncPtr done_func, void *done_data);
// void glcd_clear_screen(GLCD *glcd);
void glcd_draw_framebuffer(GLCD *glcd);
void glcd_clear_framebuffer(GLCD *glcd);

uint8_t glcd_paint_char(GLCD *glcd, char glyph, int16_t dx0, bool invert);
