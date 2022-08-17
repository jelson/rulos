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

#include "periph/ntp/ntp.h"

// Public interface used by the data uploader, which does not have to be
// specialized for any particular type of data.
class SensorDataCacheIfc {
 public:
  virtual size_t len() = 0;
  virtual void pop_n(size_t n) = 0;
  virtual size_t serialize(size_t index, char *dest, size_t max_len) = 0;
};

// Template for a cache, including a container for sensor data annotated with a
// timestamp.
template <typename SensorDataType>
class SensorDataCacheTemplate : public SensorDataCacheIfc {
 private:
  // create a container for the templated data type annotated with a timestamp
  typedef struct {
    uint64_t epoch_time_usec;
    SensorDataType data;
  } timestamped_data_t;

  timestamped_data_t *_cache;
  const size_t _capacity;
  size_t _len;
  NtpClient *_ntp;

 public:
  // constructor: allocate the requested space for the cache
  SensorDataCacheTemplate(size_t capacity, NtpClient *ntp)
      : _capacity(capacity), _len(0), _ntp(ntp) {
    _cache = new timestamped_data_t[_capacity];
    assert(_cache != NULL);
  }

  // virtual function that must be overridden: how to serialize the sensor data
  // type
  virtual char *serialize_one(SensorDataType *data) = 0;

  // generic serialize serializes the timestamp, depends on the virtual func to
  // serialize the data
  size_t serialize(size_t index, char *dest, size_t max_len) {
    assert(index < _len);
    timestamped_data_t *pt = &_cache[index];

    return snprintf(dest, max_len,
                    "%s\n"
                    "{\n"
                    "   \"time\": %llu.%06llu,\n"
                    "   %s\n"
                    "}",
                    index > 0 ? ",\n" : "", pt->epoch_time_usec / 1000000,
                    pt->epoch_time_usec % 1000000, serialize_one(&pt->data));
  }

  // remove the first n data items from the cache
  void pop_n(size_t n) {
    assert(n <= _len);
    memmove(&_cache[0], &_cache[n], sizeof(_cache[0]) * (_len - n));
    _len -= n;

    char log_msg[100];
    int log_len =
        snprintf(log_msg, sizeof(log_msg),
                 "data cache: popping %d items; %d remaining", n, _len);
    if (_len > 0) {
      log_len += snprintf(log_msg + log_len, sizeof(log_msg) - log_len,
                          " (%llu-%llu)", _cache[0].epoch_time_usec / 1000000,
                          _cache[_len - 1].epoch_time_usec / 1000000);
    }
    log_msg[log_len++] = '\n';
    log_write(log_msg, log_len);
  }

  // push a new data item into the cache. Acquire a timestamp from the NTP
  // service and either annotate the data with that timestamp, or throw the data
  // away if we don't know the time yet.
  void add(SensorDataType *data) {
    // get the current time from the NTP service
    uint64_t t = _ntp->get_epoch_time_usec();

    // print debug
    LOG("got data:time_usec=%llu.%06llu,%s", t / 1000000, t % 1000000,
        serialize_one(data));

    if (t == 0) {
      LOG("....dropping sample because NTP is not locked");
      return;
    }

    // Drop a quarter of the data if the cache has overflowed. In theory we
    // could just drop one, but that causes a lot of CPU overhead in the case
    // the network is down and the data is just getting dropped persistently.
    if (_len == _capacity) {
      pop_n(_capacity / 4);
    }

    // Add the new data item to the end
    _cache[_len].data = *data;
    _cache[_len].epoch_time_usec = t;
    _len++;

    assert(_len <= _capacity);
  }

  size_t len() {
    return _len;
  }
};
