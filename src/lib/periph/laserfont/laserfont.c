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
