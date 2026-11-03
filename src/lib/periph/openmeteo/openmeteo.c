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
#include "periph/openmeteo/openmeteo.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void openmeteo_url(char *buf, size_t buflen, float latitude, float longitude, bool fahrenheit) {
  snprintf(buf, buflen,
           "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,weather_code,is_day"
           "&daily=temperature_2m_max,temperature_2m_min"
           "&temperature_unit=%s&timezone=auto&forecast_days=1",
           latitude, longitude, fahrenheit ? "fahrenheit" : "celsius");
}

// Finds "key": in the reply at or after `from` and reads the number after
// it, which may be the first element of an array. The reply is small and
// its shape fixed, so this does instead of a JSON parser. The reply also
// has a units block naming each key with a string value, which is why
// lookups are anchored to a section.
static bool find_number(const char *from, const char *key, float *value) {
  char quoted[64];
  snprintf(quoted, sizeof(quoted), "\"%s\":", key);
  const char *p = strstr(from, quoted);
  if (p == NULL) {
    return false;
  }
  p += strlen(quoted);
  if (*p == '[') {
    p++;
  }
  char *end;
  *value = strtof(p, &end);
  return end != p;
}

static weather_condition_t condition_from_wmo_code(int code) {
  if (code == 0) {
    return WEATHER_CLEAR;
  }
  if (code <= 2) {
    return WEATHER_PARTLY_CLOUDY;
  }
  if (code == 3) {
    return WEATHER_CLOUDY;
  }
  if (code == 45 || code == 48) {
    return WEATHER_FOG;
  }
  if (code >= 51 && code <= 67) {
    return WEATHER_RAIN;  // drizzle, rain, freezing rain
  }
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
    return WEATHER_SNOW;
  }
  if (code >= 80 && code <= 82) {
    return WEATHER_RAIN;  // showers
  }
  if (code >= 95) {
    return WEATHER_THUNDERSTORM;
  }
  return WEATHER_CLOUDY;
}

bool openmeteo_parse(const char *json, weather_t *weather) {
  const char *current = strstr(json, "\"current\":");
  const char *daily = strstr(json, "\"daily\":");
  if (current == NULL || daily == NULL) {
    return false;
  }
  float temperature, code, is_day, high, low;
  if (!find_number(current, "temperature_2m", &temperature) ||
      !find_number(current, "weather_code", &code) || !find_number(current, "is_day", &is_day) ||
      !find_number(daily, "temperature_2m_max", &high) ||
      !find_number(daily, "temperature_2m_min", &low)) {
    return false;
  }
  weather->condition = condition_from_wmo_code((int)code);
  weather->is_day = is_day != 0;
  weather->temperature = (int)lroundf(temperature);
  weather->high = (int)lroundf(high);
  weather->low = (int)lroundf(low);
  return true;
}
