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

// Where and how the clock is displayed. Each has a default here and can be
// overridden at build time, e.g.
//   scons --define 'CLOCK_TZ=\"EST5EDT,M3.2.0,M11.1.0\"' --define CLOCK_LATITUDE=40.71

// POSIX TZ string for the displayed local time
#ifndef CLOCK_TZ
#define CLOCK_TZ "PST8PDT,M3.2.0,M11.1.0"
#endif

// location the weather is fetched for
#ifndef CLOCK_LATITUDE
#define CLOCK_LATITUDE 47.61
#endif
#ifndef CLOCK_LONGITUDE
#define CLOCK_LONGITUDE -122.33
#endif

// 1 for Fahrenheit, 0 for Celsius
#ifndef CLOCK_FAHRENHEIT
#define CLOCK_FAHRENHEIT 1
#endif

// how often the weather is re-fetched, and how soon after a failure
#define CLOCK_WEATHER_PERIOD_SEC 600
#define CLOCK_WEATHER_RETRY_SEC  60
