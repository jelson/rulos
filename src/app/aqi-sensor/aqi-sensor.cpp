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

// system includes
#include <stdint.h>

#include <algorithm>

// RULOS includes
#include "core/hardware.h"
#include "core/morse.h"
#include "core/rulos.h"
#include "core/watchdog.h"
#include "periph/inet/inet.h"
#include "periph/ntp/ntp.h"

// app includes
#include "cert-x3-ca.h"
#include "data-uploader.h"
#include "sensor-data-cache.h"
#include "sensor-name.h"

// static constexpr const char *BASE_URL =
// "https://secure.megabozo.com/scripts";
static constexpr const char *BASE_URL = "https://airquality.circlemud.org";
static constexpr const size_t CACHE_SIZE = 200;
static constexpr const int HTTPS_TIMEOUT_MS = 5000;
static constexpr const int WATCHDOG_TIME_SEC = 3 * 60;  // 3 minutes

UartState_t console;
watchdog_t watchdog;
HttpsClient hc(HTTPS_TIMEOUT_MS, cert_x3_ca);
SensorName sensor_name(&hc, BASE_URL);
NtpClient ntp;

//////// pms5003 variant ////////////

#if defined(AQI_PMS5003)
#include "periph/pms5003/pms5003.h"
#include "pms5003-cache.h"

pms5003_t pms;
PMS5003Cache sensor_cache(CACHE_SIZE, &ntp);

static void data_received(pms5003_data_t *data, void *user_data) {
  sensor_cache.add(data);
}
static void start_sensor() {
  pms5003_init(&pms, 1, data_received, NULL);
}
#endif

//////// dht22 variant ////////////

#if defined(AQI_DHT22)

#include "dht22-cache.h"
#include "periph/dht22/dht22.h"
#define DHT22_IO_PIN  GPIO_5
#define POLL_FREQ_SEC 15

DHT22Cache sensor_cache(CACHE_SIZE, &ntp);

static void poll_sensor(void *arg) {
  schedule_us(POLL_FREQ_SEC * 1000000, poll_sensor, arg);
  dht22_data_t data;
  if (dht22_read(DHT22_IO_PIN, &data)) {
    sensor_cache.add(&data);
  }
}

static void start_sensor() {
  // give sensor 2 sec to warm up. (datasheet says 1 sec)
  schedule_us(2000000, poll_sensor, NULL);
}
#endif

/////////

//
// Status:
//   A - alive
//   W - has wifi
//   N - has a name
//   T - has the time
//   D - has collected data
//   G - 'good' -- has sent data
//
static const char *curr_status = "A";

#define LED_PIN GPIO_2
static void blink_led(const bool led_state) {
  gpio_set_or_clr(LED_PIN, led_state);
}

static void set_status(const char *s) {
  curr_status = s;
  LOG("status now: %s", curr_status);
}

static void show_status(void *data) {
  schedule_us(4000000, show_status, data);

  switch (curr_status[0]) {
    // if we'd previously never gotten as far as wifi, but wifi is connected,
    // advanced to W
    case 'A':
      if (inet_wifi_is_connected()) {
        set_status("W");
      }
      break;

    // if we have a name, advance to N
    case 'W':
      if (sensor_name.get_sensor_name() != NULL) {
        set_status("N");
      }
      break;

    // if we have the time, advance to T
    case 'N':
      if (ntp.is_synced()) {
        set_status("T");
      }
      break;

    // if we have the time and have collected data, advance to D
    case 'T':
      if (sensor_cache.len() > 0) {
        set_status("D");
      }
      break;
  }

  LOG("emitting morse status: %s", curr_status);
  emit_morse(curr_status, 200000, blink_led, NULL);
}

void on_upload_success(void) {
  curr_status = "G";
  watchdog_keepalive(&watchdog);
}

DataUploader data_uploader(&hc, BASE_URL, &sensor_name, &sensor_cache,
                           on_upload_success);

int main() {
  rulos_hal_init();
  init_clock(10000, TIMER0);

  uart_init(&console, 0, 1000000);
  log_bind_uart(&console);
  LOG("AQI sensor running");

  watchdog_init(&watchdog, WATCHDOG_TIME_SEC);

  // status LED
  gpio_make_output(LED_PIN);
  schedule_now(show_status, NULL);

  inet_wifi_client_start(wifi_creds,
                         sizeof(wifi_creds) / sizeof(wifi_creds[0]));
  ntp.start();
  sensor_name.start();
  data_uploader.start();

  start_sensor();
  scheduler_run();
}
