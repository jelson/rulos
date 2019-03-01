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

#include <string.h>

#include "core/hal.h"
#include "periph/7seg_panel/display_controller.h"

// offset 32
static const uint8_t sevseg_ascii[] PROGMEM = {
#include "lib/periph/7seg_panel/sevseg_bitmaps.ch"
};

void display_controller_program_cell(uint8_t board, uint8_t digit,
                                     SSBitmap bitmap) {
  int segment; /* must be signed! */
  uint8_t shape = (uint8_t)bitmap;

  for (segment = 6; segment >= 0; segment--) {
    hal_program_segment(board, digit, segment, shape & 0x1);
    shape >>= 1;
  }

  /* consider the high bit to be the decimal */
  hal_program_segment(board, digit, 7, shape);
}

void display_controller_program_board(uint8_t board, SSBitmap *bitmap) {
  int i;
  for (i = 0; i < NUM_DIGITS; i++) {
    display_controller_program_cell(board, i, bitmap[i]);
  }
  display_controller_enter_sleep();
}

void display_controller_enter_sleep() { hal_7seg_bus_enter_sleep(); }

SSBitmap ascii_to_bitmap(char a) {
  if (a < ' ' || a >= 127) {
    return 0;
  }
  return pgm_read_byte(&(sevseg_ascii[((uint8_t)a) - 32]));
}

void ascii_to_bitmap_str(SSBitmap *b, int max_len, const char *a) {
  int i;
  for (i = 0; i < max_len && a[i] != '\0'; i++) {
    b[i] = ascii_to_bitmap(a[i]);
  }
}

void debug_msg_hex(uint8_t board, char *m, uint16_t hex) {
  static char buf[9];
  static SSBitmap bm[8];

  strcpy(buf, m);
  int i;
  for (i = strlen(buf); i < 8; i++) {
    buf[i] = ' ';
  }
  buf[8] = '\0';
  buf[4] = hexmap[(hex >> 12) & 0x0f];
  buf[5] = hexmap[(hex >> 8) & 0x0f];
  buf[6] = hexmap[(hex >> 4) & 0x0f];
  buf[7] = hexmap[(hex)&0x0f];
  ascii_to_bitmap_str(bm, 8, buf);
  display_controller_program_board(board, bm);
}

void board_debug_msg(uint16_t line) {
  char buf[9];
  buf[0] = 'a';
  buf[1] = 's';
  buf[2] = 'r';
  int_to_string2(&buf[3], 5, 0, line);
  buf[8] = 0;
  SSBitmap bm[8];
  ascii_to_bitmap_str(bm, 8, buf);
  display_controller_program_board(0, bm);
}
