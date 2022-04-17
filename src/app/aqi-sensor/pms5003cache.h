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
#include <stdint.h>

#include "periph/pms5003/pms5003.h"

typedef struct {
  uint64_t epoch_time_usec;
  pms5003_data_t data;
} pms5003_timestamped_t;

class PMS5003Cache {
 private:
  pms5003_timestamped_t *_cache;
  const size_t _capacity;
  size_t _len;

 public:
  PMS5003Cache(size_t capacity) : _capacity(capacity), _len(0) {
    _cache = new pms5003_timestamped_t[_capacity];
    assert(_cache != NULL);
  }

  void pop_n(size_t n) {
    assert(n <= _len);
    memmove(&_cache[0], &_cache[n], sizeof(_cache[0]) * (_len - n));
    _len -= n;
    LOG("pms5003 cache: popping %d items; %d remaining (%llu-%llu)", n, _len,
        _cache[0].epoch_time_usec / 1000000,
        _cache[_len - 1].epoch_time_usec / 1000000);
  }

  void add(pms5003_data_t *data, uint64_t epoch_time_usec) {
    // Drop a quarter of the data if the cache has overflowed. In theory we
    // could just drop one, but that causes a lot of CPU overhead in the case
    // the network is down and the data is just getting dropped persistently.
    if (_len == _capacity) {
      pop_n(_capacity / 4);
    }

    // Add the new data item to the end
    _cache[_len].data = *data;
    _cache[_len].epoch_time_usec = epoch_time_usec;
    _len++;

    assert(_len <= _capacity);
  }

  size_t len() {
    return _len;
  }

  size_t encode(size_t index, char *dest, size_t max_len) {
    assert(index < _len);
    pms5003_timestamped_t *pt = &_cache[index];

    return snprintf(dest, max_len,
                    "%s\n"
                    "{\n"
                    "   \"time\": %llu.%06llu,\n"
                    "   \"pm1.0\": %d,\n"
                    "   \"pm2.5\": %d,\n"
                    "   \"pm10.0\": %d\n"
                    "}",
                    index > 0 ? ",\n" : "", pt->epoch_time_usec / 1000000,
                    pt->epoch_time_usec % 1000000, pt->data.pm10_standard,
                    pt->data.pm25_standard, pt->data.pm100_standard);
  }
};
