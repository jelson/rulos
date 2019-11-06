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

#include <stdint.h>

#include "periph/7seg_panel/display_controller.h"

#define ROCKET6LINE_NUM_ROWS 6
#define ROCKET6LINE_NUM_COLUMNS 8

// Note that there's a single, global rocket6line object, so there's
// no instance argument to these calls.

// Initialize. Must be called before other functions.
void rocket6line_init();

// Write a single digit to the display
void rocket6line_write_digit(uint8_t row_num, uint8_t col_num, SSBitmap bm);

// Write an entire row to the display.
void rocket6line_write_line(uint8_t row_num, const SSBitmap *bm, uint8_t len);
