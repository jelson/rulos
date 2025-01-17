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

#include "periph/7seg_panel/board_config.h"

#ifndef NUM_LOCAL_BOARDS
#error "Display requested but BOARDCONFIG_xxx not defined by your app makefile."
#include <stophere>
#endif

#define NUM_DIGITS 8

/*
 * 7-segment shapes are defined as 7-bit numbers, each bit representing
 * a single segment.  From MSB to LSB (reading left to right),
 * we represent the 7 segments from A through G, like this:
 *
 *    -a-
 *  f|   |b
 *    -g-
 *  e|   |c
 *    -d-
 *
 * The decimal point is segment 7.
 */

#define SSB_DECIMAL 0x80
#define SSB_SEG_a   0x40
#define SSB_SEG_b   0x20
#define SSB_SEG_c   0x10
#define SSB_SEG_d   0x08
#define SSB_SEG_e   0x04
#define SSB_SEG_f   0x02
#define SSB_SEG_g   0x01

#define SEVSEG_BLANK 0

#define SEVSEG_0 0b1111110
#define SEVSEG_1 0b0110000
#define SEVSEG_2 0b1101101
#define SEVSEG_3 0b1111001
#define SEVSEG_4 0b0110011
#define SEVSEG_5 0b1011011
#define SEVSEG_6 0b1011111
#define SEVSEG_7 0b1110010
#define SEVSEG_8 0b1111111
#define SEVSEG_9 0b1111011

#define SEVSEG_A 0b1110111
#define SEVSEG_B 0b0011111
#define SEVSEG_C 0b1001110
#define SEVSEG_D 0b0111101
#define SEVSEG_E 0b1001111
#define SEVSEG_F 0b1000111
#define SEVSEG_G 0b1111011
#define SEVSEG_H 0b0110111
#define SEVSEG_I 0b0110000
#define SEVSEG_J 0b0111100
#define SEVSEG_K 0b0110110 /* UGLY */
#define SEVSEG_L 0b0001110
#define SEVSEG_M 0b1001111 /* also W */
#define SEVSEG_N 0b0010101
#define SEVSEG_O 0b1111110
#define SEVSEG_P 0b1100111
#define SEVSEG_Q 0b1110011
#define SEVSEG_R 0b0000101
#define SEVSEG_S 0b1011011
#define SEVSEG_T 0b0001111
#define SEVSEG_U 0b0111110
#define SEVSEG_V 0b0011100
#define SEVSEG_W 0b1001111 /* also M */
#define SEVSEG_X 0b0010100 /* UGLY */
#define SEVSEG_Y 0b0111011
#define SEVSEG_Z 0b1101001 /* UGLY */

#define SEVSEG_SPACE      0
#define SEVSEG_UNDERSCORE 0b0001000
#define SEVSEG_HYPHEN     0b0000001
#define SEVSEG_PERIOD     0b0010000
#define SEVSEG_COMMA      0b0011000

typedef uint8_t SSBitmap;

void display_controller_program_cell(uint8_t board, uint8_t digit,
                                     SSBitmap bitmap);
void display_controller_program_board(uint8_t board, SSBitmap *bitmap);
void display_controller_enter_sleep();

SSBitmap ascii_to_bitmap(char a);
void ascii_to_bitmap_str(SSBitmap *b, int max_len, const char *a);

void debug_msg_hex(uint8_t board, char *m, uint16_t hex);
#define debug_plod(m)              \
  {                                \
    debug_msg_hex(0, m, __LINE__); \
    debug_delay(250);              \
  }
void board_debug_msg(uint16_t line);
