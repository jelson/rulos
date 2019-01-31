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

int hello;
BoardLayout g_sim_theTree_def[] = {ROCKET_TREE},
            *g_sim_theTree = g_sim_theTree_def;
int goodbay;

static void sim_program_labels() {
  BoardLayout *bl;
  for (bl = g_sim_theTree; bl->label != NULL; bl += 1) {
    attroff(A_BOLD);
    wcolor_set(curses_get_window(), PAIR_WHITE, NULL);
    mvwprintw(curses_get_window(), bl->y,
              bl->x + (4 * NUM_DIGITS - strlen(bl->label)) / 2, bl->label);
  }
}

void sim_display_light_status(r_bool status) {
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

  int x_origin = g_sim_theTree[board].x + (digit)*DIGIT_WIDTH;
  int y_origin = g_sim_theTree[board].y + 1;

  x_origin += segment_defs[segment].xoff;
  y_origin += segment_defs[segment].yoff;

  if (onoff) {
    attron(A_BOLD);
    wcolor_set(curses_get_window(), g_sim_theTree[board].colors[digit], NULL);
    mvwprintw(curses_get_window(), y_origin, x_origin, segment_defs[segment].s);
  } else {
    attroff(A_BOLD);
    wcolor_set(curses_get_window(), PAIR_WHITE, NULL);
    mvwprintw(curses_get_window(), y_origin, x_origin, segment_defs[segment].s);
    //    for (int i = strlen(segment_defs[segment].s); i; i--) {
    //      mvwprintw(curses_get_window(), y_origin, x_origin + i - 1, " ");
    //    }
  }

  wrefresh(curses_get_window());
}

void hal_7seg_bus_enter_sleep() {
  // simulator can spare some power.
}

void hal_init_rocketpanel() {
  hal_init();

  /* init curses */
  init_curses();

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
