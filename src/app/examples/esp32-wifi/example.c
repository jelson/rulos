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

#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/uart/uart.h"

#define JIFFY_CLOCK_US  10000   // 10 ms jiffy clock
#define MESSAGE_FREQ_US 500000  // how often to print a message

#define TEST_PIN GPIO_2

#include "esp_wifi.h"
#include "esp_http_client.h"
#include "nvs_flash.h"

void execute_https_request(void *data);
void check_https_result(void *data);

const char cert[] = "-----BEGIN CERTIFICATE-----\n"
"MIIFYDCCBEigAwIBAgIQQAF3ITfU6UK47naqPGQKtzANBgkqhkiG9w0BAQsFADA/\n"
"MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n"
"DkRTVCBSb290IENBIFgzMB4XDTIxMDEyMDE5MTQwM1oXDTI0MDkzMDE4MTQwM1ow\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwggIiMA0GCSqGSIb3DQEB\n"
"AQUAA4ICDwAwggIKAoICAQCt6CRz9BQ385ueK1coHIe+3LffOJCMbjzmV6B493XC\n"
"ov71am72AE8o295ohmxEk7axY/0UEmu/H9LqMZshftEzPLpI9d1537O4/xLxIZpL\n"
"wYqGcWlKZmZsj348cL+tKSIG8+TA5oCu4kuPt5l+lAOf00eXfJlII1PoOK5PCm+D\n"
"LtFJV4yAdLbaL9A4jXsDcCEbdfIwPPqPrt3aY6vrFk/CjhFLfs8L6P+1dy70sntK\n"
"4EwSJQxwjQMpoOFTJOwT2e4ZvxCzSow/iaNhUd6shweU9GNx7C7ib1uYgeGJXDR5\n"
"bHbvO5BieebbpJovJsXQEOEO3tkQjhb7t/eo98flAgeYjzYIlefiN5YNNnWe+w5y\n"
"sR2bvAP5SQXYgd0FtCrWQemsAXaVCg/Y39W9Eh81LygXbNKYwagJZHduRze6zqxZ\n"
"Xmidf3LWicUGQSk+WT7dJvUkyRGnWqNMQB9GoZm1pzpRboY7nn1ypxIFeFntPlF4\n"
"FQsDj43QLwWyPntKHEtzBRL8xurgUBN8Q5N0s8p0544fAQjQMNRbcTa0B7rBMDBc\n"
"SLeCO5imfWCKoqMpgsy6vYMEG6KDA0Gh1gXxG8K28Kh8hjtGqEgqiNx2mna/H2ql\n"
"PRmP6zjzZN7IKw0KKP/32+IVQtQi0Cdd4Xn+GOdwiK1O5tmLOsbdJ1Fu/7xk9TND\n"
"TwIDAQABo4IBRjCCAUIwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYw\n"
"SwYIKwYBBQUHAQEEPzA9MDsGCCsGAQUFBzAChi9odHRwOi8vYXBwcy5pZGVudHJ1\n"
"c3QuY29tL3Jvb3RzL2RzdHJvb3RjYXgzLnA3YzAfBgNVHSMEGDAWgBTEp7Gkeyxx\n"
"+tvhS5B1/8QVYIWJEDBUBgNVHSAETTBLMAgGBmeBDAECATA/BgsrBgEEAYLfEwEB\n"
"ATAwMC4GCCsGAQUFBwIBFiJodHRwOi8vY3BzLnJvb3QteDEubGV0c2VuY3J5cHQu\n"
"b3JnMDwGA1UdHwQ1MDMwMaAvoC2GK2h0dHA6Ly9jcmwuaWRlbnRydXN0LmNvbS9E\n"
"U1RST09UQ0FYM0NSTC5jcmwwHQYDVR0OBBYEFHm0WeZ7tuXkAXOACIjIGlj26Ztu\n"
"MA0GCSqGSIb3DQEBCwUAA4IBAQAKcwBslm7/DlLQrt2M51oGrS+o44+/yQoDFVDC\n"
"5WxCu2+b9LRPwkSICHXM6webFGJueN7sJ7o5XPWioW5WlHAQU7G75K/QosMrAdSW\n"
"9MUgNTP52GE24HGNtLi1qoJFlcDyqSMo59ahy2cI2qBDLKobkx/J3vWraV0T9VuG\n"
"WCLKTVXkcGdtwlfFRjlBz4pYg1htmf5X6DYO8A4jqv2Il9DjXA6USbW1FzXSLr9O\n"
"he8Y4IWS6wY7bCkjCWDcRQJMEhg76fsO3txE+FiYruq9RUWhiF1myv4Q6W+CyBFC\n"
"Dfvp7OOGAN6dEOM4+qR9sdjoSYKEBpsr6GtPAQw4dy753ec5\n"
"-----END CERTIFICATE-----\n"
    ;

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    LOG("Wifi started, calling esp_wifi_connect");
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
    LOG("Wifi connected!");
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    LOG("Connecting to wifi failed. Retrying...");
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    LOG("got ip, bang");
    ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
    LOG("got ip: " IPSTR, IP2STR(&event->ip_info.ip));
    schedule_now(execute_https_request, NULL);
  } else {
    LOG("got unknown event base %d, event id %d", (int)event_base,
        (int)event_id);
  }
}

void start_wifi(void) {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));

  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = "Jer22",
              .password = "deadbeefff",
              .threshold.authmode = WIFI_AUTH_WPA2_PSK,

              .pmf_cfg =
                  {
                      .capable = true,
                      .required = false,
                  },
          },
  };

  LOG("Starting wifi...");
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  LOG("Wifi started.");
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
  switch (evt->event_id) {
  case HTTP_EVENT_ERROR:
    LOG("Error on http operation");
    break;
  case HTTP_EVENT_ON_HEADER:
    LOG("got an http header: key=%s, value=%s", evt->header_key, evt->header_value);
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

void execute_https_request(void *data) {
  esp_http_client_config_t config = {
        .url = "https://secure.megabozo.com/scripts/reverse",
        .event_handler = _http_event_handler,
        .cert_pem = cert,
        .is_async = true,
        .timeout_ms = 5000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    const char *post_data = "ABCDE12345";
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    schedule_now(check_https_result, client);
}

void check_https_result(void *data) {
  esp_http_client_handle_t client = (esp_http_client_handle_t) data;
  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_ERR_HTTP_EAGAIN) {
    schedule_us(100000, check_https_result, data);
    return;
  }

  if (err == ESP_OK) {
    LOG("HTTPS Status = %d, content_length = %d",
        esp_http_client_get_status_code(client),
        esp_http_client_get_content_length(client));
  } else {
    LOG("Error perform http request %s", esp_err_to_name(err));
  }
  esp_http_client_cleanup(client);
}

int main() {
  rulos_hal_init();
  init_clock(JIFFY_CLOCK_US, TIMER0);

  UartState_t u;
  uart_init(&u, /* uart_id= */ 0, 38400);
  log_bind_uart(&u);

  // TODO move this to esp32 hal init
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  start_wifi();

  cpumon_main_loop();
}
