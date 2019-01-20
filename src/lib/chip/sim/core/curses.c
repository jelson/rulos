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

#include "core/curses.h"

static WINDOW* mainwnd;

void init_curses() {
  mainwnd = initscr();
  start_color();
  init_pair(PAIR_BLUE, COLOR_BLUE, COLOR_BLACK);
  init_pair(PAIR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
  init_pair(PAIR_GREEN, COLOR_GREEN, COLOR_BLACK);
  init_pair(PAIR_RED, COLOR_RED, COLOR_BLACK);
  init_pair(PAIR_WHITE, COLOR_WHITE, COLOR_BLACK);
  init_pair(PAIR_BLACK_ON_WHITE, COLOR_BLACK, COLOR_WHITE);
  // attron(COLOR_PAIR(1));
  noecho();
  cbreak();
  nodelay(mainwnd, TRUE);
  refresh();
  curs_set(0);
}

WINDOW* curses_get_window() { return mainwnd; }
