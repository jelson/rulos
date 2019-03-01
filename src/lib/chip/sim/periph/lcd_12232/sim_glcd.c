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

#include <ctype.h>
#include <curses.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "chip/sim/core/sim.h"
#include "core/rulos.h"
#include "core/util.h"
#include "periph/7seg_panel/display_controller.h"
#include "periph/lcd_12232/graphic_lcd_12232.h"

/************* glcd ********************/

void glcd_init(GLCD *glcd, ActivationFuncPtr done_func, void *done_data) {}

void glcd_draw_framebuffer(GLCD *glcd) {}

void glcd_clear_framebuffer(GLCD *glcd) {}

uint8_t glcd_paint_char(GLCD *glcd, char glyph, int16_t dx0, r_bool invert) {
  return 0;
}
