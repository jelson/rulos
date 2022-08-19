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

#include "periph/inet/inet.h"

#include <stdio.h>
#include <stdlib.h>

#include <algorithm>

#include "core/hardware.h"
#include "core/rulos.h"
#include "core/util.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const inet_wifi_creds_t *g_wifi_creds = NULL;
static int g_num_wifi_creds = 0;
static int g_wifi_creds_idx = -1;

const char *curr_wifi_ssid() {
  return g_wifi_creds[g_wifi_creds_idx].ssid;
}

static void reconfigure_wifi_creds() {
  assert(g_num_wifi_creds > 0);

  // find the next set of credentials to try
  g_wifi_creds_idx++;
  if (g_wifi_creds_idx == g_num_wifi_creds) {
    g_wifi_creds_idx = 0;
  }

  LOG("Wifi: Trying to connect to wifi SSID %s", curr_wifi_ssid());

  wifi_config_t wifi_config = {.sta = {
                                   .threshold =
                                       {
                                           .authmode = WIFI_AUTH_WPA2_PSK,
                                       },
                                   .pmf_cfg =
                                       {
                                           .capable = true,
                                           .required = false,
                                       },
                               }};
  strcpy((char *)wifi_config.sta.ssid, curr_wifi_ssid());
  strcpy((char *)wifi_config.sta.password,
         g_wifi_creds[g_wifi_creds_idx].password);

  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
}

static bool _is_connected = false;

bool inet_wifi_is_connected() {
  return _is_connected;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    LOG("Wifi: started");
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
    LOG("Wifi: connected to %s", curr_wifi_ssid());
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    LOG("Wifi: connection to %s failed", curr_wifi_ssid());
    _is_connected = false;
    reconfigure_wifi_creds();
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    _is_connected = true;
    LOG("Wifi: Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
  } else {
    LOG("Wifi: got unknown event base %d, event id %d", (int)event_base,
        (int)event_id);
  }
}

static char _macaddr_string[20] = {};

const char *inet_wifi_macaddr() {
  if (!*_macaddr_string) {
    uint8_t binary_mac[6];
    esp_wifi_get_mac((wifi_interface_t)ESP_MAC_WIFI_STA, binary_mac);
    snprintf(_macaddr_string, sizeof(_macaddr_string),
             "%02X:%02X:%02X:%02X:%02X:%02X", binary_mac[0], binary_mac[1],
             binary_mac[2], binary_mac[3], binary_mac[4], binary_mac[5]);
  }
  return _macaddr_string;
}

void inet_wifi_client_start(const inet_wifi_creds_t *wifi_creds,
                            int num_creds) {
  // store credentials for later
  assert(g_wifi_creds == NULL);
  assert(g_num_wifi_creds == 0);
  assert(num_creds > 0);
  g_wifi_creds = wifi_creds;
  g_num_wifi_creds = num_creds;

  // init ESP32's wifi API
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();
  wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  reconfigure_wifi_creds();
  ESP_ERROR_CHECK(esp_wifi_start());
}

/////////////////////////////////////////////////////////////
////////////////////// https client /////////////////////////
/////////////////////////////////////////////////////////////

HttpsClient::HttpsClient(int timeout_ms, const char *cert) {
  _timeout_ms = timeout_ms;
  _cert = cert;
  _in_use = false;
  _terminate = false;
  _client = NULL;
  _worker_thread_running = false;
}

void HttpsClient::_maybeStartWorkerThread() {
  if (_worker_thread_running) {
    return;
  }

  // Create semaphores used for inter-task coordination
  _req_ready = xSemaphoreCreateBinary();
  _worker_ready = xSemaphoreCreateBinary();

  // Create a task to perform the blocking HTTPS requests.
  xTaskCreate(HttpsClient::_worker_thread_trampoline, "https_client", 8 * 1024,
              this, configMAX_PRIORITIES, NULL);

  // Wait for task to start
  xSemaphoreTake(_worker_ready, portMAX_DELAY);
  _worker_thread_running = true;
  LOG("HTTPS Client: worker started");
}

HttpsClient::~HttpsClient() {
  // notify the worker it's time to go to terminate
  if (_worker_thread_running) {
    _terminate = true;
    xSemaphoreGive(_req_ready);

    // wait for the worker to tell us it's done
    xSemaphoreTake(_worker_ready, portMAX_DELAY);
  }
}

bool HttpsClient::is_in_use() {
  return _in_use;
}

void HttpsClient::set_header(const char *header, const char *value) {
  _maybeStartWorkerThread();
  ESP_ERROR_CHECK(esp_http_client_set_header(_client, header, value));
}

void HttpsClient::set_response_buffer(char *buf, size_t len) {
  _response_buffer = buf;
  _response_buffer_len = len;
}

esp_err_t HttpsClient::_event_handler_trampoline(esp_http_client_event_t *evt) {
  HttpsClient *c = static_cast<HttpsClient *>(evt->user_data);
  return c->_event_handler(evt);
}

esp_err_t HttpsClient::_event_handler(esp_http_client_event_t *evt) {
  switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
      LOG("HTTPS Client: Error on http operation");
      break;
    case HTTP_EVENT_ON_HEADER:
      LOG("HTTPS Client: got an http header: key=%s, value=%s", evt->header_key,
          evt->header_value);
      break;
    case HTTP_EVENT_ON_DATA: {
      size_t data_to_store =
          r_min((size_t)evt->data_len,
                _response_buffer_len - _response_bytes_written);
      memcpy(_response_buffer + _response_bytes_written, evt->data,
             data_to_store);
      _response_bytes_written += data_to_store;
    }
    default:
      break;
  }

  return ESP_OK;
}

void HttpsClient::get(const char *url, HttpsHandlerIfc *on_done) {
  post(url, NULL, 0, on_done);
}

void HttpsClient::post(const char *url, const char *post_body, size_t body_len,
                       HttpsHandlerIfc *on_done) {
  _maybeStartWorkerThread();

  assert(!_in_use);
  assert(_response_buffer != NULL);
  assert(_response_buffer_len != 0);
  _response_bytes_written = 0;
  _on_done = on_done;
  _in_use = true;

  ESP_ERROR_CHECK(esp_http_client_set_url(_client, url));

  if (post_body != NULL) {
    esp_http_client_set_method(_client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(_client, post_body, body_len);
  } else {
    esp_http_client_set_method(_client, HTTP_METHOD_GET);
  }

  xSemaphoreGive(_req_ready);
}

void HttpsClient::_create_esp32_client_object() {
  LOG("HTTPS Client: creating client object");

  esp_http_client_config_t config = {
      .url = "https://localhost",  // temporary; set on get/post
      .cert_pem = _cert,
      .timeout_ms = _timeout_ms,
      .event_handler = HttpsClient::_event_handler_trampoline,
      .user_data = this,
      .is_async = false,
  };

  // ensure we're not leaking client objects
  assert(_client == NULL);

  // create the client object
  _client = esp_http_client_init(&config);

  // ensure the client object creation did not fail
  assert(_client != NULL);
}

void HttpsClient::_destroy_esp32_client_object() {
  LOG("HTTPS Client: destroying client object");
  assert(_client != NULL);
  esp_http_client_cleanup(_client);
  _client = NULL;
}

// little trampoline until the RULOS scheduler understands C++
// natively
void HttpsClient::_worker_thread_trampoline(void *context) {
  HttpsClient *hc = static_cast<HttpsClient *>(context);
  hc->_worker_thread();
}

void HttpsClient::_worker_thread() {
  // create an http object so that set_header works
  _create_esp32_client_object();

  // tell the main thread that the worker is ready
  xSemaphoreGive(_worker_ready);

  while (true) {
    xSemaphoreTake(_req_ready, portMAX_DELAY);

    if (_terminate) {
      LOG("HTTPS Client: terminating worker thread");
      _destroy_esp32_client_object();
      xSemaphoreGive(_worker_ready);
      return;
    }

    _perform_one_op();
  }
}

// warning: runs on a separate task!
void HttpsClient::_perform_one_op() {
  LOG("HTTPS Client: executing request");

  esp_err_t err = esp_http_client_perform(_client);

  if (err == ESP_OK) {
    _result_code = esp_http_client_get_status_code(_client);
    LOG("HTTPS Client: request complete: status=%d, content_length=%d",
        _result_code, esp_http_client_get_content_length(_client));
  } else {
    _result_code = -1;
    LOG("HTTPS Client: error: %s", esp_err_to_name(err));

    // destroy and re-create client; some failures leave the client in a state
    // where it can't be reused and never recovers
    _destroy_esp32_client_object();
    _create_esp32_client_object();
  }

  // invoke the done callback from the main rulos thread
  schedule_now(HttpsClient::_invoke_done_callback, this);
}

void HttpsClient::_invoke_done_callback(void *context) {
  HttpsClient *hc = static_cast<HttpsClient *>(context);

  hc->_in_use = false;
  hc->_on_done->on_done(hc, hc->_result_code, hc->_response_bytes_written);
}
