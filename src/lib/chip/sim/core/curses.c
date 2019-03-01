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
