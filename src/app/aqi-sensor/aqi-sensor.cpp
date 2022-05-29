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
#include "periph/inet/inet.h"
#include "periph/ntp/ntp.h"
#include "periph/pms5003/pms5003.h"

// app includes
#include "cert-x3-ca.h"
#include "data-uploader.h"
#include "pms5003cache.h"
#include "sensor-name.h"

// static constexpr const char *BASE_URL =
// "https://secure.megabozo.com/scripts";
static constexpr const char *BASE_URL = "https://airquality.circlemud.org";
static constexpr const size_t CACHE_SIZE = 200;
static constexpr const int HTTPS_TIMEOUT_MS = 5000;

UartState_t console;
HttpsClient hc(HTTPS_TIMEOUT_MS, cert_x3_ca);
SensorName sensor_name(&hc, BASE_URL);
NtpClient ntp;
pms5003_t pms;
PMS5003Cache pms_cache(CACHE_SIZE);
DataUploader data_uploader(&hc, BASE_URL, &sensor_name, &pms_cache);

static void data_received(pms5003_data_t *data, void *user_data) {
  uint64_t t = ntp.get_epoch_time_usec();

  LOG("data:time_usec=%llu.%06llu,pm1.0=%d,pm2.5=%d,pm10.0=%d", t / 1000000,
      t % 1000000, data->pm10_standard, data->pm25_standard,
      data->pm100_standard);

  // drop samples that are collected before we have ntp lock
  if (t != 0) {
    pms_cache.add(data, t);
  } else {
    LOG("...dropping sample because NTP is not locked");
  }
}

/////////

int main() {
  rulos_hal_init();
  init_clock(10000, TIMER0);

  uart_init(&console, 0, 1000000);
  log_bind_uart(&console);
  LOG("AQI sensor running");

  inet_wifi_client_start(wifi_creds,
                         sizeof(wifi_creds) / sizeof(wifi_creds[0]));
  hc.set_header("Content-Type", "application/json");
  ntp.start();
  sensor_name.start();
  data_uploader.start();

  pms5003_init(&pms, 1, data_received, NULL);
  scheduler_run();
}
