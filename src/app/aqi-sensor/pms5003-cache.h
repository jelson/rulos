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

#include "sensor-data-cache.h"

class PMS5003Cache : public SensorDataCacheTemplate<pms5003_data_t> {
 private:
  char _buf[100];

 public:
  PMS5003Cache(size_t capacity, NtpClient *ntp)
      : SensorDataCacheTemplate<pms5003_data_t>(capacity, ntp) {
  }

  virtual char *serialize_one(pms5003_data_t *data) {
    snprintf(_buf, sizeof(_buf),
             "\"pm1.0\": %d,"
             "\"pm2.5\": %d,"
             "\"pm10.0\": %d",
             data->pm10_standard, data->pm25_standard, data->pm100_standard);
    return _buf;
  }
};
