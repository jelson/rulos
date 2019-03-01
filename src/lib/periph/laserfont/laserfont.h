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

#include <inttypes.h>

#include "periph/rocket/rocket.h"

#define LASERFONT_LOWEST_CHAR (32)
#define LASERFONT_HIGHEST_CHAR (126)

typedef struct {
  uint8_t widthpairs[(LASERFONT_HIGHEST_CHAR - LASERFONT_LOWEST_CHAR) / 2 + 1];
  SSBitmap data;
} LaserFont;

uint8_t laserfont_draw_char(LaserFont *lf, SSBitmap *bm, int size, char c);
int laserfont_draw_string(LaserFont *lf, SSBitmap *bm, int size, char *s);
