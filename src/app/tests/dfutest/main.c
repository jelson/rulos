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

// Minimal RULOS USB CDC app that exists only to exercise the shared
// DFU firmware-update facility (core/dfu.*, plus the DFU runtime
// interface the usb-cdc-stm32 peripheral adds to its USB descriptor).
//
// It does NOTHING DFU-specific itself -- it just initializes USB CDC,
// which is the whole point: any app that uses USB CDC inherits the
// dfu-util -> ROM-bootloader update path. The app emits a one-line
// banner once a second so the host test can read which firmware
// revision is currently running:
//
//   DFUTEST FW=<version>\n
//
// dfu_test.py flashes "Rev A" via the Black Magic Probe, reads the
// banner, then runs dfu-util to update to "Rev B" purely over USB and
// confirms the banner changed.

#include <stdio.h>

#include "chip/arm/stm32/periph/usb_cdc/usb_cdc.h"
#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/uart/uart.h"

#ifndef DFUTEST_FW_VERSION
#error "DFUTEST_FW_VERSION not set -- build with --define (see dfu_test.py)"
#endif

static usbd_cdc_state_t usb_cdc;
static UartState_t uart;
static char banner[64];
static char connect_banner[64];

static void on_connect(usbd_cdc_state_t *cdc, void *user_data) {
  // Greet the moment the host opens the port, so a reader sees the
  // revision immediately instead of waiting up to a second. Distinct
  // prefix so it's unambiguous which path produced a given line.
  if (usbd_cdc_tx_ready(&usb_cdc)) {
    usbd_cdc_write(&usb_cdc, connect_banner, strlen(connect_banner));
  }
}

static void banner_task(void *data) {
  schedule_us(1000000, banner_task, NULL);
  if (usbd_cdc_tx_ready(&usb_cdc)) {
    usbd_cdc_write(&usb_cdc, banner, strlen(banner));
  }
}

int main() {
  rulos_hal_init();
  init_clock(10000, TIMER1);

  uart_init(&uart, /* uart_id= */ 0, 1000000);
  log_bind_uart(&uart);
  LOG("dfutest starting, FW=%s", DFUTEST_FW_VERSION);

  snprintf(banner, sizeof(banner), "DFUTEST FW=%s\n", DFUTEST_FW_VERSION);
  snprintf(connect_banner, sizeof(connect_banner),
           "INIT DFUTEST FW=%s\n", DFUTEST_FW_VERSION);

  usb_cdc = (usbd_cdc_state_t){
      .connect_cb = on_connect,
      .user_data = NULL,
  };
  usbd_cdc_init(&usb_cdc);

  schedule_now(banner_task, NULL);
  scheduler_run();
}
