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
#include <stdint.h>

extern "C" {
#include "esp_http_client.h"
}

typedef struct {
  const char *ssid;
  const char *password;
} inet_wifi_creds_t;

void inet_wifi_client_start(const inet_wifi_creds_t *wifi_creds, int num_creds);
const char *inet_wifi_macaddr();

class HttpsClient;

class HttpsHandlerIfc {
 public:
  virtual void on_done(HttpsClient *hc, int response_code,
                       size_t response_len) = 0;
};

class HttpsClient {
 public:
  HttpsClient(int timeout_ms, const char *cert);
  ~HttpsClient();
  void set_header(const char *header, const char *value);
  void set_response_buffer(char *buf, size_t len);
  void get(const char *url, HttpsHandlerIfc *on_done);
  void post(const char *url, const char *post_body, size_t body_len,
            HttpsHandlerIfc *on_done);

 private:
  bool _in_use;
  const char *_cert;
  int _timeout_ms;
  char *_response_buffer;
  size_t _response_buffer_len;
  size_t _response_bytes_written;
  esp_http_client_handle_t _client;
  HttpsHandlerIfc *_on_done;

  static esp_err_t _event_handler_trampoline(esp_http_client_event_t *evt);
  esp_err_t _event_handler(esp_http_client_event_t *evt);
  static void _check_https_result_trampoline(void *context);
  void _check_https_result();
};
