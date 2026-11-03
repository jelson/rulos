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

// ESP32 IDF includes
#include "driver/uart.h"
#include "soc/soc_caps.h"
#include "soc/uart_pins.h"
#if SOC_USB_SERIAL_JTAG_SUPPORTED
#include "driver/usb_serial_jtag.h"
#endif

// note: uart driver fails to install if this is too small! (128 caused a
// failure)
#define UART_BUFLEN 2048

// How long a write to the USB Serial/JTAG port waits for the host to drain
// the buffer before giving up. The port looks connected but accepts no data
// when the host has the device enumerated and nothing reading it, and a log
// line must not stall the scheduler.
#define USB_SERIAL_JTAG_WRITE_TIMEOUT_MS 20

typedef struct esp32_uart esp32_uart_t;

// The ways a RULOS uart id can reach the outside world: a hardware UART, or
// the USB Serial/JTAG peripheral on chips that have one.
typedef struct {
  void (*install)(esp32_uart_t *eu, uint32_t baud);
  void (*set_baud)(esp32_uart_t *eu, uint32_t baud);
  void (*write)(esp32_uart_t *eu, const char *buf, size_t len);
  // Blocks until at least one byte has arrived, then returns up to maxlen of
  // them.
  int (*read)(esp32_uart_t *eu, char *buf, size_t maxlen);
} serial_ops_t;

struct esp32_uart {
  const serial_ops_t *ops;
  uart_port_t esp32_uart_num;
  int rx_pin;
  int tx_pin;

  uint8_t rulos_uart_id;
  hal_uart_next_sendbuf_cb tx_cb;
  void *cb_data;

  // rx
  hal_uart_receive_cb rx_cb;
  char *rx_buf;
  uint32_t rx_buflen;
  bool rx_outstanding;
  QueueHandle_t event_queue;
};

//////////////// hardware uarts /////////////////////

static void hw_uart_install(esp32_uart_t *eu, uint32_t baud) {
  uart_config_t uart_config = {};
  uart_config.baud_rate = (int)baud;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uart_config.source_clk = UART_SCLK_DEFAULT;
  ESP_ERROR_CHECK(uart_driver_install(eu->esp32_uart_num, /*rx_buflen=*/UART_BUFLEN,
                                      /*tx_buflen=*/UART_BUFLEN, /* queue_size */ 20,
                                      &eu->event_queue,
                                      /*intr_alloc_flags */ 0));
  ESP_ERROR_CHECK(uart_param_config(eu->esp32_uart_num, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(eu->esp32_uart_num, eu->tx_pin, eu->rx_pin, UART_PIN_NO_CHANGE,
                               UART_PIN_NO_CHANGE));
}

static void hw_uart_set_baud(esp32_uart_t *eu, uint32_t baud) {
  ESP_ERROR_CHECK(uart_set_baudrate(eu->esp32_uart_num, baud));
}

static void hw_uart_write(esp32_uart_t *eu, const char *buf, size_t len) {
  uart_write_bytes(eu->esp32_uart_num, buf, len);
}

static int hw_uart_read(esp32_uart_t *eu, char *buf, size_t maxlen) {
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

  return uart_read_bytes(eu->esp32_uart_num, buf, maxlen, 0);
}

static const serial_ops_t hw_uart_ops = {
    .install = hw_uart_install,
    .set_baud = hw_uart_set_baud,
    .write = hw_uart_write,
    .read = hw_uart_read,
};

//////////////// usb serial/jtag /////////////////////

#if SOC_USB_SERIAL_JTAG_SUPPORTED

static void usb_serial_install(esp32_uart_t *eu, uint32_t baud) {
  usb_serial_jtag_driver_config_t config = {};
  config.tx_buffer_size = UART_BUFLEN;
  config.rx_buffer_size = UART_BUFLEN;
  ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&config));
}

static void usb_serial_set_baud(esp32_uart_t *eu, uint32_t baud) {
  // USB has no line rate
}

static void usb_serial_write(esp32_uart_t *eu, const char *buf, size_t len) {
  if (!usb_serial_jtag_is_connected()) {
    return;
  }
  usb_serial_jtag_write_bytes(buf, len, pdMS_TO_TICKS(USB_SERIAL_JTAG_WRITE_TIMEOUT_MS));
}

static int usb_serial_read(esp32_uart_t *eu, char *buf, size_t maxlen) {
  return usb_serial_jtag_read_bytes(buf, maxlen, portMAX_DELAY);
}

static const serial_ops_t usb_serial_ops = {
    .install = usb_serial_install,
    .set_baud = usb_serial_set_baud,
    .write = usb_serial_write,
    .read = usb_serial_read,
};

#endif  // SOC_USB_SERIAL_JTAG_SUPPORTED

//////////////// uart table /////////////////////

// Hardware UARTs use the chip's default pins. A UART with no default pin
// (UART2 on the ESP32-S3) is left unconnected.
static esp32_uart_t esp32_uart[] = {
    {
        .ops = &hw_uart_ops,
        .esp32_uart_num = UART_NUM_0,
        .rx_pin = U0RXD_GPIO_NUM,
        .tx_pin = U0TXD_GPIO_NUM,
    },
    {
        .ops = &hw_uart_ops,
        .esp32_uart_num = UART_NUM_1,
        .rx_pin = U1RXD_GPIO_NUM,
        .tx_pin = U1TXD_GPIO_NUM,
    },
    {
        .ops = &hw_uart_ops,
        .esp32_uart_num = UART_NUM_2,
        .rx_pin = U2RXD_GPIO_NUM,
        .tx_pin = U2TXD_GPIO_NUM,
    },
#if SOC_USB_SERIAL_JTAG_SUPPORTED
    {
        .ops = &usb_serial_ops,
    },
#endif
};

#define NUM_UARTS (sizeof(esp32_uart) / sizeof(esp32_uart[0]))

static esp32_uart_t *get_uart(uint8_t uart_id) {
  assert(uart_id < NUM_UARTS);
  esp32_uart_t *eu = &esp32_uart[uart_id];
  eu->rulos_uart_id = uart_id;
  return eu;
}

void hal_uart_init(uint8_t uart_id, uint32_t baud, void *user_data /* for both rx and tx upcalls */,
                   size_t *max_tx_len /* OUT */) {
  esp32_uart_t *eu = get_uart(uart_id);

  // save RULOS callback data and tell uart layer the max size of writes we're
  // willing to acccept
  eu->cb_data = user_data;
  *max_tx_len = UART_BUFLEN;

  eu->ops->install(eu, baud);
}

void hal_uart_set_baud(uint8_t uart_id, uint32_t baud) {
  esp32_uart_t *eu = get_uart(uart_id);
  eu->ops->set_baud(eu, baud);
}

void hal_uart_start_send(uint8_t uart_id, hal_uart_next_sendbuf_cb cb) {
  esp32_uart_t *eu = get_uart(uart_id);

  while (true) {
    uint16_t this_send_len;
    const char *this_buf;
    cb(uart_id, eu->cb_data, &this_buf, &this_send_len);
    if (this_send_len == 0) {
      return;
    }
    eu->ops->write(eu, this_buf, this_send_len);
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

    const int num_read = eu->ops->read(eu, eu->rx_buf, eu->rx_buflen / 2);

    // send up to rulos-land
    if (num_read > 0) {
      eu->rx_outstanding = true;
      eu->rx_cb(eu->rulos_uart_id, eu->cb_data, eu->rx_buf, num_read);
    }
  }
}

void hal_uart_start_rx(uint8_t uart_id, hal_uart_receive_cb rx_cb, void *buf, size_t buflen) {
  assert(rx_cb != NULL);
  assert(buf != NULL);
  assert(buflen >= 2 && buflen % 2 == 0);
  esp32_uart_t *eu = get_uart(uart_id);
  eu->rx_cb = rx_cb;
  eu->rx_buf = (char *)buf;
  eu->rx_buflen = buflen;
  xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, eu, configMAX_PRIORITIES, NULL);
}

void hal_uart_rx_cb_done(uint8_t uart_id) {
  esp32_uart_t *eu = get_uart(uart_id);
  eu->rx_outstanding = false;
}
