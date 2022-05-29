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

#include "core/rulos.h"
#include "periph/inet/inet.h"
#include "wifi-credentials.h"

class SensorName : public HttpsHandlerIfc {
 private:
  HttpsClient *_hc;
  const char *_base_url;
  char _sensor_name[100];
  bool _valid;

  static constexpr const char *SENSOR_NAME_URL = "mac_lookup";
  static constexpr const int RETRY_TIME_SEC = 5;

  static void _resolve_sensor_name_trampoline(void *data) {
    SensorName *sn = static_cast<SensorName *>(data);
    sn->_resolve_sensor_name();
  }

  void _resolve_sensor_name() {
    if (_valid) {
      return;
    }

    // schedule the next retry just in case this one fails
    schedule_us(1000000 * RETRY_TIME_SEC, _resolve_sensor_name_trampoline,
                this);

    // if last request is outstanding, do nothing
    if (_hc->is_in_use()) {
      LOG("get sensor name: previous req still outstanding");
      return;
    }

    // set aside one byte for NULL -- we'll null terminate data
    // in-place after we get it
    _hc->set_response_buffer(_sensor_name, sizeof(_sensor_name) - 1);
    char url[100];
    snprintf(url, sizeof(url), "%s/%s?macaddr=%s", _base_url, SENSOR_NAME_URL,
             inet_wifi_macaddr());
    LOG("Getting sensor name at: %s", url);
    _hc->get(url, this);
  }

  void on_done(HttpsClient *hc, int response_code, size_t response_len) {
    if (response_code == 200 && response_len >= 1) {
      LOG("Sensor name retrieval: success, code %d, len %d", response_code,
          response_len);
      _sensor_name[response_len] = '\0';
      _valid = true;
      LOG("Retrieved sensor name: %s", get_sensor_name());
    } else {
      LOG("Sensor name retrieval: failed, code %d", response_code);
    }
  }

 public:
  SensorName(HttpsClient *hc, const char *base_url) {
    _hc = hc;
    _base_url = base_url;
    _valid = false;
  }

  const char *get_sensor_name() {
    if (_valid) {
      return _sensor_name;
    } else {
      return NULL;
    }
  }

  void start() {
    schedule_now(_resolve_sensor_name_trampoline, this);
  }
};
