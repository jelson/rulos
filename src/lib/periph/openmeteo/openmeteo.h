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
#include <stddef.h>

#include "periph/weather/weather.h"

// Open-Meteo (open-meteo.com): a free forecast service that needs no API key
// and answers over plain HTTP, so a device needs no certificates to use it.

#define OPENMETEO_URL_LEN 256

// Writes the request for a location's current conditions and today's low
// and high, in Fahrenheit or Celsius.
void openmeteo_url(char *buf, size_t buflen, float latitude, float longitude, bool fahrenheit);

// Parses the service's JSON reply to that request. Returns false if a
// field is missing.
bool openmeteo_parse(const char *json, weather_t *weather);
