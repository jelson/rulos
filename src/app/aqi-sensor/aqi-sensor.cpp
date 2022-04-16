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

#include "cert_airquality.circlemud.org.h"
#include "core/rulos.h"
#include "periph/inet/inet.h"
#include "periph/pms5003/pms5003.h"
#include "sensor_name.h"

static void data_received(pms5003_data_t *data, void *user_data) {
  LOG("data:pm1.0=%d,pm2.5=%d,pm10.0=%d", data->pm10_standard,
      data->pm25_standard, data->pm100_standard);
}

/////////

static constexpr const char *BASE_URL = "https://airquality.circlemud.org";

UartState_t console;
HttpsClient hc;
pms5003_t pms;
SensorName sensor_name(&hc, BASE_URL);

int main() {
  rulos_hal_init();
  init_clock(10000, TIMER0);

  uart_init(&console, 0, 1000000);
  log_bind_uart(&console);
  LOG("AQI sensor running");

  inet_wifi_client_start(wifi_creds,
                         sizeof(wifi_creds) / sizeof(wifi_creds[0]));
  hc.set_timeout_ms(5000);
  hc.set_https_cert(cert_airquality_circlemud_org);

  sensor_name.start();

  pms5003_init(&pms, 1, data_received, NULL);
  scheduler_run();
}
