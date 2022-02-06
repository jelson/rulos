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
#include "periph/inet/inet.h"
#include "periph/ntp/ntp.h"
#include "periph/uart/uart.h"

// esp32-specific includes needed only for catching pin interrupts
#include "driver/gpio.h"

#define JIFFY_CLOCK_US 10000  // 10 ms jiffy clock
const gpio_num_t EXT_REF_PIN_NUM = (gpio_num_t)GPIO_27;

// An example of the contents of this file is found in
// wifi-credentials-example.h. Copy it to your home directory,
// ~/.config/rulos/wifi-credentials.h, and fill it in with real data.
#include "wifi-credentials.h"

Time next_print_time;
// NtpClient ntp("time.gin.ntt.net");
NtpClient ntp("seiko.s.uw.edu");

void print_timestamp(const char *source) {
  uint64_t epoch, local;
  ntp.get_epoch_and_local_usec(&epoch, &local);

  if (epoch == 0) {
    LOG("%s: NTP not locked", source);
  } else {
    LOG("%s: ntp timestamp: epoch_time=%llu.%06llu, local=%llu", source,
        epoch / 1000000, epoch % 1000000, local);
  }
}

void external_gpio_handler(void *arg) {
  print_timestamp("External GPIO");
}

static void show_time(void *arg) {
  print_timestamp("Internal 1hz");
  next_print_time += 1000000;
  schedule_absolute(next_print_time, show_time, arg);
}

int main() {
  rulos_hal_init();

  UartState_t u;
  uart_init(&u, /* uart_id= */ 0, 1000000);
  log_bind_uart(&u);

  init_clock(JIFFY_CLOCK_US, TIMER0);

  // start wifi
  inet_wifi_client_start(wifi_creds,
                         sizeof(wifi_creds) / sizeof(wifi_creds[0]));

  // start ntp
  ntp.start();

  // print our estimate of the time once per second
  next_print_time = clock_time_us();
  schedule_absolute(next_print_time, show_time, NULL);

  gpio_make_input_enable_pullup(EXT_REF_PIN_NUM);
  // install a handler to print the time on an external reference event
  gpio_set_intr_type(EXT_REF_PIN_NUM, GPIO_INTR_POSEDGE);
  gpio_isr_handler_add(EXT_REF_PIN_NUM, external_gpio_handler, NULL);
  gpio_intr_enable(EXT_REF_PIN_NUM);

  cpumon_main_loop();
}
