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
#include "core/curses.h"
#include "core/util.h"
#include "periph/7seg_panel/board_config.h"
#include "periph/7seg_panel/display_controller.h"
#include "periph/ring_buffer/rocket_ring_buffer.h"
#include "periph/rocket/rocket.h"
#include "periph/uart/uart.h"

/*
   __
  |__|
  |__|.

seg  x  y char
0    1  0 __
1    3  1 |
2    3  2 |
3    1  2 __
4    0  2 |
5    0  1 |
6    1  1 __
7    4  2 .

*/

#define DIGIT_WIDTH 5
#define DIGIT_HEIGHT 4
#define SHOW_OFF_SEGMENTS 0

struct segment_def_s {
  int xoff;
  int yoff;
  const char *s;
} segment_defs[] = {{1, 0, "__"}, {3, 1, "|"}, {3, 2, "|"},  {1, 2, "__"},
                    {0, 2, "|"},  {0, 1, "|"}, {1, 1, "__"}, {4, 2, "."}};

void hal_upside_down_led(SSBitmap *b) {}

#define DBOARD(name, syms, x, y, remote_addr, remote_idx) \
  { name, {syms}, x, y }
#define B_NO_BOARD {NULL},
#define B_END \
  { NULL }
#include "periph/7seg_panel/display_tree.ch"

BoardLayout g_sim_theTree_def[] = {ROCKET_TREE},
            *g_sim_theTree = g_sim_theTree_def;

// Current status of each displayed segment. -1 means uninitialized. 0
// means off. 1 means on.
static int curr_segment_status[NUM_LOCAL_BOARDS][NUM_DIGITS][8];

static void sim_program_labels() {
  BoardLayout *bl;
  for (bl = g_sim_theTree; bl->label != NULL; bl += 1) {
    attroff(A_BOLD);
    wcolor_set(curses_get_window(), PAIR_WHITE, NULL);
    mvwprintw(curses_get_window(),
              bl->y,
              bl->x + (4 * NUM_DIGITS - strlen(bl->label)) / 2,
              "%s", bl->label);
  }
}

void sim_display_light_status(bool status) {
  if (status) {
    wcolor_set(curses_get_window(), PAIR_BLACK_ON_WHITE, NULL);
    mvwprintw(curses_get_window(), 17, 50, " Lights on  ");
  } else {
    wcolor_set(curses_get_window(), PAIR_WHITE, NULL);
    mvwprintw(curses_get_window(), 17, 50, " Lights off ");
  }
}

void hal_program_segment(uint8_t board, uint8_t digit, uint8_t segment,
                         uint8_t onoff) {
  if (board < 0 || board >= NUM_LOCAL_BOARDS || digit < 0 ||
      digit >= NUM_DIGITS || segment < 0 || segment >= 8 ||
      g_sim_theTree[board].label == NULL)
    return;

  if (curr_segment_status[board][digit][segment] == onoff) {
    return;
  }

  curr_segment_status[board][digit][segment] = onoff;

  int x_origin = g_sim_theTree[board].x + (digit)*DIGIT_WIDTH;
  int y_origin = g_sim_theTree[board].y + 1;

  x_origin += segment_defs[segment].xoff;
  y_origin += segment_defs[segment].yoff;

  if (onoff) {
    attron(A_BOLD);
    attroff(A_DIM);
    wcolor_set(curses_get_window(), g_sim_theTree[board].colors[digit], NULL);
    mvwprintw(curses_get_window(), y_origin, x_origin,
              "%s", segment_defs[segment].s);
  } else {
#if SHOW_OFF_SEGMENTS
    attroff(A_BOLD);
    attron(A_DIM);
    wcolor_set(curses_get_window(), PAIR_WHITE, NULL);
    mvwprintw(curses_get_window(), y_origin, x_origin, segment_defs[segment].s);
#else
    for (int i = strlen(segment_defs[segment].s); i; i--) {
      mvwprintw(curses_get_window(), y_origin, x_origin + i - 1, " ");
    }
#endif
  }

  wrefresh(curses_get_window());
}

void hal_7seg_bus_enter_sleep() {
  // simulator can spare some power.
}

void hal_init_rocketpanel() {
  /* init curses */
  init_curses();

  for (int board = 0; board < NUM_LOCAL_BOARDS; board++) {
    for (int digit = 0; digit < NUM_DIGITS; digit++) {
      for (int segment = 0; segment < 8; segment++) {
        curr_segment_status[board][digit][segment] = -1;
      }
    }
  }

  /* init screen */
  sim_program_labels();
  SSBitmap value = ascii_to_bitmap('8') | SSB_DECIMAL;
  int board, digit;
  for (board = 0; board < NUM_LOCAL_BOARDS; board++) {
    for (digit = 0; digit < NUM_DIGITS; digit++) {
      display_controller_program_cell(board, digit, value);
    }
  }
}
