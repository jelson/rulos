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
#include "nvs_flash.h"

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
