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

#include "periph/dht22/dht22.h"
#include "sensor-data-cache.h"

class DHT22Cache : public SensorDataCacheTemplate<dht22_data_t> {
 private:
  char _buf[100];

 public:
  DHT22Cache(size_t capacity, NtpClient *ntp)
      : SensorDataCacheTemplate<dht22_data_t>(capacity, ntp) {
  }

  virtual char *serialize_one(dht22_data_t *data) {
    snprintf(_buf, sizeof(_buf),
             "\"temperature_C\": %.1f,"
             "\"humidity_perc\": %.1f",
             data->temp_c_tenths / 10.0, data->humidity_pct_tenths / 10.0);
    return _buf;
  }
};
