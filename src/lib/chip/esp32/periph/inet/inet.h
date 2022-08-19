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

#include "core/time.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern "C" {
#include "esp_http_client.h"
}

typedef struct {
  const char *ssid;
  const char *password;
} inet_wifi_creds_t;

void inet_wifi_client_start(const inet_wifi_creds_t *wifi_creds, int num_creds);
const char *inet_wifi_macaddr();
bool inet_wifi_is_connected();

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
  bool is_in_use();
  void set_header(const char *header, const char *value);
  void set_response_buffer(char *buf, size_t len);
  void get(const char *url, HttpsHandlerIfc *on_done);
  void post(const char *url, const char *post_body, size_t body_len,
            HttpsHandlerIfc *on_done);

 private:
  // params from the caller
  const char *_cert;
  int _timeout_ms;
  HttpsHandlerIfc *_on_done;

  // state of the client
  bool _worker_thread_running;
  bool _in_use;
  bool _terminate;
  SemaphoreHandle_t _req_ready;
  SemaphoreHandle_t _worker_ready;

  // state of the request
  esp_http_client_handle_t _client;
  char *_response_buffer;
  size_t _response_buffer_len;
  size_t _response_bytes_written;
  int _result_code;

  void _destroy_esp32_client_object();
  void _create_esp32_client_object();
  static esp_err_t _event_handler_trampoline(esp_http_client_event_t *evt);
  esp_err_t _event_handler(esp_http_client_event_t *evt);
  static void _worker_thread_trampoline(void *context);
  void _maybeStartWorkerThread();
  void _worker_thread();
  void _perform_one_op();
  static void _invoke_done_callback(void *context);
};
