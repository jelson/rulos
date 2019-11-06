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

#include "periph/rasters/rasters.h"

#include "core/rulos.h"

void raster_big_digit_update(RasterBigDigit *digit);

#include "rasters_auto.ch"

uint16_t get_bitfield(uint16_t offset, uint8_t len) {
  uint8_t bitsAvaiableThisByte = 8 - (offset % 8);
  uint8_t bitsUsedThisByte = min(bitsAvaiableThisByte, len);
  uint8_t byte = pgm_read_byte(&(rasterData[offset / 8]));
  uint16_t v = byte >> (bitsAvaiableThisByte - bitsUsedThisByte);
  v &= ((1 << bitsUsedThisByte) - 1);
  uint16_t outVal = (v << (len - bitsUsedThisByte));

  uint8_t rest_len = len - bitsUsedThisByte;
  uint16_t rest_offset = offset + bitsUsedThisByte;
  if (rest_len > 0) {
    assert(rest_offset > offset);
    assert((rest_offset % 8) == 0);
    outVal |= get_bitfield(rest_offset, rest_len);
  }
  return outVal;
}

void raster_draw_sym(RectRegion *rrect, char sym, int8_t dx, int8_t dy) {
  const RasterIndex *ri = rasterIndex;
  while (ri->sym != '\0' && ri->sym != sym) {
    ri++;
  }
  if (ri->sym == '\0') {
    LOG("sym = %d (%c)", sym, sym);
    assert(FALSE);  // symbol not found
    return;
  }

  int y = 0;
  for (int idx = 0; idx < ri->len; idx++) {
    uint16_t bitfield = get_bitfield(ri->offset_bits + idx * 11, 11);
    uint8_t ybit = (bitfield >> 10) & 1;
    uint8_t x0 = (bitfield >> 5) & 0x1f;
    uint8_t x1 = (bitfield >> 0) & 0x1f;
    if (ybit) {
      y += 1;
    }
    // LOG("raster_draw_sym(%c idx %2d/%2d) y%d x0 %2d x1 %2d", sym, idx,
    // ri->len, y, x0, x1);
    for (int x = x0; x < x1; x++) {
      // LOG("paintpixel(%d,%d)", dx+x, dy+y);
      raster_paint_pixel(rrect, dx + x, dy + y);
    }
  }
}

static SSBitmap _sevseg_pixel_mask[6][4] = {
    {0b00000000, 0b01000000, 0b00000000, 0b00000000},
    {0b00000010, 0b00000000, 0b00100000, 0b00000000},
    {0b00000000, 0b00000001, 0b00000000, 0b00000000},
    {0b00000100, 0b00000000, 0b00010000, 0b00000000},
    {0b00000000, 0b00001000, 0b00000000, 0b10000000},
    {0b00000000, 0b00000000, 0b00000000, 0b00000000}};

void raster_paint_pixel(RectRegion *rrect, int x, int y) {
  raster_paint_pixel_v(rrect, x, y, TRUE);
}

void raster_paint_pixel_v(RectRegion *rrect, int x, int y, r_bool on) {
  int maj_x = int_div_with_correct_truncation(x, 4);
  int min_x = x - (maj_x * 4);
  int maj_y = int_div_with_correct_truncation(y, 6);
  int min_y = y - (maj_y * 6);

  // LOG("x-painted y%2d.%d x%2d.%d", maj_y, min_y, maj_x, min_x);
  if (maj_y < 0 || maj_y >= rrect->ylen || maj_x < rrect->x ||
      maj_x >= rrect->x + rrect->xlen) {
    // LOG("discard: %d %d ; %d %d", 0, rrect->ylen, rrect->x,
    // rrect->x+rrect->xlen);
    return;
  }

  // LOG("PAINT!");
  if (on) {
    rrect->bbuf[maj_y]->buffer[maj_x] |= _sevseg_pixel_mask[min_y][min_x];
  } else {
    rrect->bbuf[maj_y]->buffer[maj_x] &= ~(_sevseg_pixel_mask[min_y][min_x]);
  }
}

void raster_big_digit_init(RasterBigDigit *digit, Screen4 *s4) {
  digit->startTime = clock_time_us();
  digit->s4 = s4;
  digit->focused = FALSE;

  schedule_us(1, (ActivationFuncPtr)raster_big_digit_update, digit);
}

void raster_big_digit_update(RasterBigDigit *digit) {
  schedule_us(50000, (ActivationFuncPtr)raster_big_digit_update, digit);

  if (!digit->focused) {
    // this thing is EXPENSIVE! don't draw it to offscreen buffers!
    // Also, don't abuse shared s4 surface.
    return;
  }

  RectRegion *rrect = &digit->s4->rrect;
  raster_clear_buffers(rrect);

  const int spacing = 28;
  const int roll = 10;
  Time t = -(clock_time_us() - digit->startTime) / 1000;
  while (t < 0) {
    t += 1000000;
  }
  int tens = ((t / 10000) % 10);
  int ones = ((t / 1000) % 10);
  int ones_offset =
      min((t * (spacing + roll) / 1000) % (spacing + roll), spacing);
  int tens_offset = 0;
  if (ones == 9) {
    tens_offset = ones_offset;
  }
  for (int i = -1; i <= 1; i++) {
    int curr_digit = (tens + i + 10) % 10;
    raster_draw_sym(rrect, '0' + curr_digit, 0, tens_offset - (spacing * i));
  }
  for (int i = -1; i <= 1; i++) {
    int curr_digit = (ones + i + 10) % 10;
    raster_draw_sym(rrect, '0' + curr_digit, 16, ones_offset - (spacing * i));
  }

  raster_draw_buffers(rrect);
}

void raster_clear_buffers(RectRegion *rrect) {
  for (int row = 0; row < rrect->ylen; row++) {
    memset(rrect->bbuf[row]->buffer + rrect->x, 0, rrect->xlen);
  }
}

void raster_draw_buffers(RectRegion *rrect) {
  for (int row = 0; row < rrect->ylen; row++) {
    board_buffer_draw(rrect->bbuf[row]);
  }
}
