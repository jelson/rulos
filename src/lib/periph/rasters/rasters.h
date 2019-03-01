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
#include "periph/7seg_panel/region.h"
#include "periph/rocket/screen4.h"

typedef struct {
  char sym;
  uint8_t len;           // units: 11-bit fields
  uint16_t offset_bits;  // units: bits
} RasterIndex;

extern const RasterIndex rasterIndex[];
extern const uint8_t rasterData[];

void raster_draw_sym(RectRegion *rrect, char sym, int8_t dx, int8_t dy);
void raster_paint_pixel(RectRegion *rrect, int x, int y);
void raster_paint_pixel_v(RectRegion *rrect, int x, int y, r_bool on);
// x in [0,32), y in [0,24)

typedef struct {
  Screen4 *s4;
  Time startTime;
  r_bool focused;
} RasterBigDigit;

void raster_big_digit_init(RasterBigDigit *digit, Screen4 *s4);
void raster_clear_buffers(RectRegion *rrect);
void raster_draw_buffers(RectRegion *rrect);
