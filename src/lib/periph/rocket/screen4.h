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

#include "periph/7seg_panel/board_buffer.h"
#include "periph/rocket/rocket.h"

#if defined(BOARDCONFIG_UNIROCKET) || defined(BOARDCONFIG_UNIROCKET_LOCALSIM)
#define SCREEN4SIZE 6
#else
#define SCREEN4SIZE 4 /* shocking, I know */
#endif

typedef struct {
  uint8_t board0;
  BoardBuffer bbuf[SCREEN4SIZE];
  BoardBuffer *bbufp[SCREEN4SIZE];
  RectRegion rrect;
} Screen4;

void init_screen4(Screen4 *s4, uint8_t board0);
void s4_show(Screen4 *s4);
void s4_hide(Screen4 *s4);
void s4_draw(Screen4 *s4);
bool s4_visible(Screen4 *s4);
