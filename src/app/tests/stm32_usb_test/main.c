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

#include <stdio.h>

#include "chip/arm/stm32/periph/usb_cdc/usb_cdc.h"
#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/uart/uart.h"

// USB CDC device state
static usbd_cdc_state_t usb_cdc;

// TX buffer (caller owns)
static char tx_buf[256];
static uint32_t counter = 0;

// UART for logging
static UartState_t uart;

static void usb_rx_handler(usbd_cdc_state_t *cdc, void *user_data,
                           const uint8_t *data, uint32_t len) {
  LOG("USB RX: %lu bytes: '%.*s'", len, (int)len, data);
}

static void usb_tx_complete(usbd_cdc_state_t *cdc, void *user_data) {
  // TX done, can reuse tx_buf now if needed
}

static void hello_task(void *data) {
  // Schedule next message in 1 second
  schedule_us(1000000, hello_task, NULL);

  // Send periodic hello message
  if (!usbd_cdc_tx_ready(&usb_cdc)) {
    LOG("USB CDC not ready (initted=%d usb_ready=%d tx_busy=%d)",
        usb_cdc.initted, usb_cdc.usb_ready, usb_cdc.tx_busy);
    return;
  }

  int len = snprintf(tx_buf, sizeof(tx_buf),
                     "Hello from STM32 CDC! Message #%lu uptime=%lu ms\n",
                     counter++, clock_time_us() / 1000);

  if (len > 0 && len < sizeof(tx_buf)) {
    usbd_cdc_write(&usb_cdc, tx_buf, len);
  }
}

int main() {
  rulos_hal_init();
  init_clock(10000, TIMER1);

  uart_init(&uart, /* uart_id= */ 0, 1000000);
  log_bind_uart(&uart);

  LOG("STM32G4 USB CDC example starting!");

  // Configure USB CDC device
  usb_cdc = (usbd_cdc_state_t){
      .rx_cb = usb_rx_handler,
      .tx_complete_cb = usb_tx_complete,
      .user_data = NULL,
  };

  usbd_cdc_init(&usb_cdc);
  schedule_now(hello_task, NULL);
  scheduler_run();
}
