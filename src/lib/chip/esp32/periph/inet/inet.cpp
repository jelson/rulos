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

#include "core/rulos.h"
#include "esp_wifi.h"

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

  LOG("Trying to connect to wifi SSID %s", curr_wifi_ssid());

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
    reconfigure_wifi_creds();
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    LOG("Wifi: Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
  } else {
    LOG("Wifi: got unknown event base %d, event id %d", (int)event_base,
        (int)event_id);
  }
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

/////// https client

HttpsClient::HttpsClient() {
  _in_use = false;
}

void HttpsClient::set_response_buffer(char *buf, size_t len) {
  _response_buffer = buf;
  _response_buffer_len = len;
}

void HttpsClient::set_timeout_ms(int timeout_ms) {
  _timeout_ms = timeout_ms;
}

void HttpsClient::set_https_cert(const char *cert) {
  _cert = cert;
}

esp_err_t HttpsClient::_event_handler_trampoline(esp_http_client_event_t *evt) {
  HttpsClient *c = static_cast<HttpsClient *>(evt->user_data);
  return c->_event_handler(evt);
}

esp_err_t HttpsClient::_event_handler(esp_http_client_event_t *evt) {
  switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
      LOG("Error on http operation");
      break;
    case HTTP_EVENT_ON_HEADER:
      LOG("got an http header: key=%s, value=%s", evt->header_key,
          evt->header_value);
      break;
    case HTTP_EVENT_ON_DATA:
      LOG("got http response data:");
      log_write(evt->data, evt->data_len);
      LOG("");
    default:
      break;
  }

  return ESP_OK;
}

// little trampoline until the RULOS scheduler understands C++
// natively
void HttpsClient::_check_https_result_trampoline(void *context) {
  HttpsClient *c = static_cast<HttpsClient *>(context);
  c->_check_https_result();
}

void HttpsClient::_check_https_result() {
  esp_err_t err = esp_http_client_perform(_client);

  if (err == ESP_ERR_HTTP_EAGAIN) {
    schedule_us(100000, HttpsClient::_check_https_result_trampoline, this);
    return;
  }

  if (err == ESP_OK) {
    LOG("HTTPS Status = %d, content_length = %d",
        esp_http_client_get_status_code(_client),
        esp_http_client_get_content_length(_client));
  } else {
    LOG("Error performing http request %s", esp_err_to_name(err));
  }
  esp_http_client_cleanup(_client);
  _in_use = false;
  on_done();
}

void HttpsClient::post(const char *url, const char *post_body,
                       size_t body_len) {
  assert(!_in_use);
  assert(_response_buffer != NULL);
  assert(_response_buffer_len != 0);
  _response_bytes_written = 0;
  _in_use = true;

  esp_http_client_config_t config = {
      .url = url,
      .cert_pem = _cert,
      .timeout_ms = _timeout_ms,
      .event_handler = HttpsClient::_event_handler_trampoline,
      .user_data = this,
      .is_async = true,
  };

  _client = esp_http_client_init(&config);

  if (post_body != NULL) {
    esp_http_client_set_method(_client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(_client, post_body, body_len);
  } else {
    esp_http_client_set_method(_client, HTTP_METHOD_GET);
  }

  schedule_us(100000, HttpsClient::_check_https_result_trampoline, this);
}

void HttpsClient::get(const char *url) {
  post(url, NULL, 0);
}
