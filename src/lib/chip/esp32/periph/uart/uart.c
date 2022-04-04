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

// note: uart driver fails to install if this is too small! (128 caused a
// failure)
#define UART_BUFLEN 2048

typedef struct {
  uart_port_t esp32_uart_num;
  uint8_t rulos_uart_id;
  int rx_pin;
  int tx_pin;
  hal_uart_next_sendbuf_cb tx_cb;
  void *cb_data;

  // rx
  hal_uart_receive_cb rx_cb;
  char *rx_buf;
  uint32_t rx_buflen;
  bool rx_outstanding;
  QueueHandle_t event_queue;
} esp32_uart_t;

#define NUM_UARTS 3

static esp32_uart_t esp32_uart[NUM_UARTS] = {
    {
        .esp32_uart_num = UART_NUM_0,
        .rulos_uart_id = 0,
        .rx_pin = 3,
        .tx_pin = 1,
    },
    {
        .esp32_uart_num = UART_NUM_1,
        .rulos_uart_id = 1,
        .rx_pin = 9,
        .tx_pin = 10,
    },
    {
        .esp32_uart_num = UART_NUM_2,
        .rulos_uart_id = 2,
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
  ESP_ERROR_CHECK(uart_driver_install(
      eu->esp32_uart_num, /*rx_buflen=*/UART_BUFLEN,
      /*tx_buflen=*/UART_BUFLEN, /* queue_size */ 20, &eu->event_queue,
      /*intr_alloc_flags */ 0));
  ESP_ERROR_CHECK(uart_param_config(eu->esp32_uart_num, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(eu->esp32_uart_num, eu->tx_pin, eu->rx_pin,
                               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
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

//////////////// receiving /////////////////////

// We're going to cheat on the esp32 and just spin up a task that blocks on uart
// input.

static void rx_task(void *arg) {
  esp32_uart_t *eu = (esp32_uart_t *)arg;
  while (true) {
    // wait until the app has finished processing the previous data
    while (eu->rx_outstanding) {
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    // If there's no data available, wait for an event indicating some has
    // arrived
    size_t bytes_avail = 0;
    uart_get_buffered_data_len(eu->esp32_uart_num, &bytes_avail);
    while (bytes_avail == 0) {
      uart_event_t event;
      xQueueReceive(eu->event_queue, &event, (TickType_t)portMAX_DELAY);
      // we could inspect event here and see if it's a data-received type, but
      // not much point
      uart_get_buffered_data_len(eu->esp32_uart_num, &bytes_avail);
    }

    // read data from esp32 uart driver's queue
    const int num_read =
        uart_read_bytes(eu->esp32_uart_num, eu->rx_buf, eu->rx_buflen, 0);

    // send up to rulos-land
    if (num_read > 0) {
      eu->rx_outstanding = true;
      eu->rx_cb(eu->rulos_uart_id, eu->cb_data, eu->rx_buf, num_read);
    }
  }
}

void hal_uart_start_rx(uint8_t uart_id, hal_uart_receive_cb rx_cb, void *buf,
                       size_t buflen) {
  assert(uart_id >= 0 && uart_id < NUM_UARTS);
  esp32_uart_t *eu = &esp32_uart[uart_id];
  eu->rx_cb = rx_cb;
  eu->rx_buf = (char *)buf;
  eu->rx_buflen = buflen;
  xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, eu, configMAX_PRIORITIES,
              NULL);
}

void hal_uart_rx_cb_done(uint8_t uart_id) {
  assert(uart_id >= 0 && uart_id < NUM_UARTS);
  esp32_uart_t *eu = &esp32_uart[uart_id];
  eu->rx_outstanding = false;
}
