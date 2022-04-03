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

// rulos includes
#include "core/hal.h"
#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/uart/uart_hal.h"

// esp32 includes
#include "driver/uart.h"

typedef struct {
  uart_port_t esp32_uart_num;
  int rx_pin;
  int tx_pin;
  hal_uart_next_sendbuf_cb tx_cb;
  hal_uart_receive_cb rx_cb;
  void *cb_data;
} esp32_uart_t;

#define NUM_UARTS   3
#define UART_BUFLEN 2048

static esp32_uart_t esp32_uart[NUM_UARTS] = {
    {
        .esp32_uart_num = UART_NUM_0,
        .rx_pin = 3,
        .tx_pin = 1,
    },
    {
        .esp32_uart_num = UART_NUM_1,
        .rx_pin = 9,
        .tx_pin = 10,
    },
    {
        .esp32_uart_num = UART_NUM_2,
        .rx_pin = 16,
        .tx_pin = 17,
    },
};

void hal_uart_init(uint8_t uart_id, uint32_t baud,
                   void *user_data /* for both rx and tx upcalls */,
                   size_t *max_tx_len /* OUT */) {
  assert(uart_id >= 0 && uart_id < NUM_UARTS);
  esp32_uart_t *eu = &esp32_uart[uart_id];

  // save RULOS callback data and tell uart layer the max size of writes we're
  // willing to acccept
  eu->cb_data = user_data;
  *max_tx_len = UART_BUFLEN;

  const uart_config_t uart_config = {
      .baud_rate = (int)baud,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_APB,
  };
  ESP_ERROR_CHECK(
      uart_driver_install(eu->esp32_uart_num, /*rx_buflen=*/UART_BUFLEN,
                          /*tx_buflen=*/UART_BUFLEN, /* queue_size */ 0,
                          /* queue handle */ NULL,
                          /*intr_alloc_flags */ 0));
  ESP_ERROR_CHECK(uart_param_config(eu->esp32_uart_num, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(eu->esp32_uart_num, eu->tx_pin, eu->rx_pin,
                               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

void hal_uart_start_rx(uint8_t uart_id, hal_uart_receive_cb rx_cb, void *buf,
                       size_t buflen) {
  assert(false);  // not implemented yet
}

void hal_uart_rx_cb_done(uint8_t uart_id) {
  assert(false);  // not implemented yet
}

void hal_uart_start_send(uint8_t uart_id, hal_uart_next_sendbuf_cb cb) {
  assert(uart_id >= 0 && uart_id < NUM_UARTS);
  esp32_uart_t *eu = &esp32_uart[uart_id];

  while (true) {
    uint16_t this_send_len;
    const char *this_buf;
    cb(uart_id, eu->cb_data, &this_buf, &this_send_len);
    if (this_send_len == 0) {
      return;
    }
    uart_write_bytes(eu->esp32_uart_num, this_buf, this_send_len);
  }
}
