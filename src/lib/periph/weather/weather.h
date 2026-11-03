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

#include <stdbool.h>

// A weather report in the terms a display cares about, independent of the
// service it came from.

typedef enum {
  WEATHER_CLEAR,
  WEATHER_PARTLY_CLOUDY,
  WEATHER_CLOUDY,
  WEATHER_FOG,
  WEATHER_RAIN,
  WEATHER_SNOW,
  WEATHER_THUNDERSTORM,
  WEATHER_CONDITION_COUNT,
} weather_condition_t;

typedef struct {
  weather_condition_t condition;
  bool is_day;
  // temperatures in whatever unit the report was requested in
  int temperature;
  int low;
  int high;
} weather_t;

// e.g. "Partly cloudy"
const char *weather_condition_name(weather_condition_t condition);
