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
  schedule_now(poll_sensor, NULL);
}
#endif

/////////

DataUploader data_uploader(&hc, BASE_URL, &sensor_name, &sensor_cache,
                           &watchdog);

int main() {
  rulos_hal_init();
  init_clock(10000, TIMER0);

  uart_init(&console, 0, 1000000);
  log_bind_uart(&console);
  LOG("AQI sensor running");

  watchdog_init(&watchdog, WATCHDOG_TIME_SEC);

  inet_wifi_client_start(wifi_creds,
                         sizeof(wifi_creds) / sizeof(wifi_creds[0]));
  ntp.start();
  sensor_name.start();
  data_uploader.start();

  start_sensor();
  scheduler_run();
}
