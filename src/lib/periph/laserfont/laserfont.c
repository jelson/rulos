/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
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

#include "periph/laserfont/laserfont.h"

#include "laserfont.ch"

#define LF_CHAR_WIDTH(lf, i) \
  (((lf->widthpairs[i >> 1]) >> ((1 - (i & 1)) << 2)) & 0x0f)

uint8_t laserfont_draw_char(LaserFont *lf, SSBitmap *bm, int size, char c) {
  if (c < LASERFONT_LOWEST_CHAR || c > LASERFONT_HIGHEST_CHAR) {
    return 0;
  }

  uint16_t offset = 0;
  uint8_t ci;
  for (ci = 0; ci < (c - LASERFONT_LOWEST_CHAR); ci++) {
    offset += LF_CHAR_WIDTH(lf, ci);
  }
  uint8_t glyph_width = LF_CHAR_WIDTH(lf, ci);

  uint8_t w = 0;
  SSBitmap *glyph_bm = lf->data + offset;
  while (w < glyph_width && size > 0) {
    *bm = *glyph_bm;
    bm++;
    glyph_bm++;
    w++;
    size--;
  }
  return w;
}

int laserfont_draw_string(LaserFont *lf, SSBitmap *bm, int size, char *s) {
  int total_w = 0;
  uint8_t w;
  while ((size > 0) && ((*s) != '\0')) {
    w = laserfont_draw_char(lf, bm, size, *s);
    bm += w;
    size -= w;
    s++;
    total_w += w;
  }
  return total_w;
}
