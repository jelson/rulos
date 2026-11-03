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

#include <time.h>

#include "periph/weather/weather.h"

// The clock's screen, in LVGL alone: it knows nothing about where the time
// or weather come from or what it is drawn on. A host creates the screen
// once on the active LVGL display, then feeds it.
void clock_ui_create(void);

// Shows a local time, or placeholders when t is NULL (time not yet known).
void clock_ui_set_time(const struct tm *t);

// Shows a weather report, or hides the weather when it is NULL.
void clock_ui_set_weather(const weather_t *weather);

// One line of small text at the bottom: sync state, errors, and so on.
void clock_ui_set_status(const char *status);
