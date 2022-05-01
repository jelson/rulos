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

// rulos includes
#include "core/rulos.h"
#include "periph/inet/inet.h"

// app includes
#include "data-uploader.h"
#include "pms5003cache.h"
#include "sensor-name.h"

// config
#include "wifi-credentials.h"

class DataUploader : public HttpsHandlerIfc {
 private:
  HttpsClient *_hc;
  const char *_base_url;
  SensorName *_sn;
  PMS5003Cache *_cache;

  static constexpr const char *DATA_UPLOAD_URL = "data";
  static const uint32_t UPLOAD_FREQ_SEC = 15;
  static const size_t MAX_POINTS_PER_UPLOAD = 100;
  size_t _num_outstanding;
  size_t _json_max_size;
  char *_jsonbuf;
  char _respbuf[10000];

  void _encode() {
    char *p = _jsonbuf;
    int cap = _json_max_size;

    size_t len = snprintf(p, cap,
                          "{\n"
                          "   \"clowny-cleartext-password\": \"%s\",\n"
                          "   \"sensorname\": \"%s\",\n"
                          "   \"sensordata\": [\n",
                          lectrobox_aqi_password,  // from wifi-credentials.h
                          _sn->get_sensor_name());
    p += len;
    cap -= len;

    for (size_t i = 0; i < _num_outstanding; i++) {
      len = _cache->encode(i, p, cap);
      p += len;
      cap -= len;
      assert(cap > 0);
    }

    snprintf(p, cap, "]}");
  }

  void _upload() {
    // if a previous upload is still in process, do not upload
    if (_num_outstanding > 0) {
      LOG("not uploading: upload still outstanding");
      return;
    }

    // if we haven't resolved the sensor name yet, do not upload
    if (_sn->get_sensor_name() == NULL) {
      LOG("not uploading: unresolved sensor name");
      return;
    }

    // how many points should we upload?
    _num_outstanding = std::min(MAX_POINTS_PER_UPLOAD, _cache->len());
    if (_num_outstanding == 0) {
      LOG("not uploading: no data");
      return;
    }

    // encode them to json
    LOG("preparing %d records for upload", _num_outstanding);
    _encode();

    // post
    _hc->set_response_buffer(_respbuf, sizeof(_respbuf));
    char url[100];
    snprintf(url, sizeof(url), "%s/%s", _base_url, DATA_UPLOAD_URL);
    LOG("posting to %s", url);
    log_write(_jsonbuf, strlen(_jsonbuf));
    log_write("\n", 1);
    _hc->post(url, _jsonbuf, strlen(_jsonbuf), this);
  }

  static void _upload_trampoline(void *data) {
    schedule_us(UPLOAD_FREQ_SEC * 1000000, _upload_trampoline, data);
    DataUploader *du = static_cast<DataUploader *>(data);
    du->_upload();
  }

  void on_done(HttpsClient *hc, int response_code, size_t response_len) {
    log_write(_respbuf, response_len);
    if (response_code == 200) {
      LOG("Data upload: success!");
      _cache->pop_n(_num_outstanding);
    } else {
      LOG("Data upload: failed, code %d", response_code);
    }
    _num_outstanding = 0;
  }

 public:
  DataUploader(HttpsClient *hc, const char *base_url, SensorName *sn,
               PMS5003Cache *cache)
      : _hc(hc),
        _base_url(base_url),
        _sn(sn),
        _cache(cache),
        _num_outstanding(0) {
    _json_max_size = MAX_POINTS_PER_UPLOAD * 100;
    _jsonbuf = new char[_json_max_size];
    assert(_jsonbuf != NULL);
  }

  void start() {
    schedule_us(1, _upload_trampoline, this);
  }
};
