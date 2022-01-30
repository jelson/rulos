/*
41;363;0c * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
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
#include "periph/inet/inet.h"
#include "periph/uart/uart.h"
#include "periph/ntp/ntp.h"

#define JIFFY_CLOCK_US 10000  // 10 ms jiffy clock

// An example of the contents of this file is found in
// wifi-credentials-example.h. Copy it to your home directory,
// ~/.config/rulos/wifi-credentials.h, and fill it in with real data.
#include "wifi-credentials.h"

NtpClient ntp;

void show_time(void *arg) {
  schedule_us(1000000, show_time, arg);
  if (ntp.is_synced()) {
    LOG("NTP epoch time is %d sec, %lld ms", ntp.get_epoch_time_sec(),
        ntp.get_epoch_time_msec());
  } else {
    //LOG("NTP not synchronized");
  }
}
  
int main() {
  rulos_hal_init();
  init_clock(JIFFY_CLOCK_US, TIMER0);

  UartState_t u;
  uart_init(&u, /* uart_id= */ 0, 38400);
  log_bind_uart(&u);

  inet_wifi_client_start(wifi_creds,
                         sizeof(wifi_creds) / sizeof(wifi_creds[0]));
  ntp.start();
  schedule_now(show_time, NULL);

  cpumon_main_loop();
}
