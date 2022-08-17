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

/*
 * Basic test program that frobs a GPIO; should compile for all platforms. No
 * peripherals. Meant as a basic test of the build system for new platforms and
 * chips.
 */

#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/dht22/dht22.h"

#define DHT22_IO_PIN GPIO_5

static void tick(void *arg) {
  schedule_us(1000000, tick, NULL);
  dht22_data_t d;
  if (dht22_read(DHT22_IO_PIN, &d)) {
    LOG("Got data: Temp %.1f, humidity %.1f%%", d.temp_c_tenths / 10.0,
        d.humidity_pct_tenths / 10.0);
  } else {
    LOG("Couldn't read from DHT22 :(");
  }
}

int main() {
  UartState_t uart;
  rulos_hal_init();
  uart_init(&uart, /* uart_id= */ 0, 1000000);
  log_bind_uart(&uart);
  LOG("Log output running!");

  init_clock(10000, TIMER0);
  schedule_now(tick, NULL);
  scheduler_run();
}
