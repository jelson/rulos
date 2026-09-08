// Exercise the actual ESP32 UART HAL against a deterministic ESP-IDF task/driver fixture.
#include <assert.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/uart.h"
#include "periph/uart/uart.h"
#include "periph/uart/uart_hal.h"

static void (*receive_task)(void *);
static void *receive_task_arg;
static UartRxBuffer_t storage;
static unsigned task_creations;
static unsigned reads;
static unsigned callbacks;
static unsigned acknowledgements;
static unsigned delays;
static unsigned events;
static size_t bytes_left;
static bool expect_assertion;
static jmp_buf assertion_jump;
static jmp_buf task_done;

void log_assert(const char *file, unsigned long line) {
  if (expect_assertion) {
    longjmp(assertion_jump, 1);
  }
  fprintf(stderr, "%s:%lu: assertion failed\n", file, line);
  abort();
}

int uart_driver_install(uart_port_t port, int rx_len, int tx_len, int queue_len,
                        QueueHandle_t *queue, int flags) {
  assert(port == UART_NUM_0 && rx_len > 0 && tx_len > 0 && queue_len > 0 && flags == 0);
  *queue = &events;
  return 0;
}
int uart_param_config(uart_port_t port, const uart_config_t *config) {
  assert(port == UART_NUM_0 && config->baud_rate == 115200);
  return 0;
}
int uart_set_pin(uart_port_t port, int tx, int rx, int rts, int cts) {
  assert(port == UART_NUM_0 && tx == 1 && rx == 3);
  assert(rts == UART_PIN_NO_CHANGE && cts == UART_PIN_NO_CHANGE);
  return 0;
}
int xTaskCreate(void (*task)(void *), const char *name, unsigned stack_size, void *arg,
                unsigned priority, void *handle) {
  assert(strcmp(name, "uart_rx_task") == 0 && stack_size > 0);
  assert(priority == configMAX_PRIORITIES && handle == NULL);
  task_creations++;
  receive_task = task;
  receive_task_arg = arg;
  return 1;
}
int uart_get_buffered_data_len(uart_port_t port, size_t *len) {
  assert(port == UART_NUM_0);
  assert(callbacks == acknowledgements);
  if (bytes_left == 0) {
    longjmp(task_done, 1);
  }
  *len = events == 0 ? 0 : bytes_left;
  return 0;
}
int xQueueReceive(QueueHandle_t queue, void *event, TickType_t wait) {
  assert(queue == &events && event != NULL && wait == portMAX_DELAY);
  events++;
  return 1;
}
int uart_read_bytes(uart_port_t port, void *buf, uint32_t len, TickType_t wait) {
  assert(port == UART_NUM_0 && buf == storage.rx_queue && wait == 0);
  assert(len == UART_RX_QUEUE_LEN / 2);
  assert(reads == callbacks && callbacks == acknowledgements);
  reads++;
  size_t count = bytes_left < len ? bytes_left : len;
  memset(buf, 'A' + callbacks, count);
  bytes_left -= count;
  return (int)count;
}
static void receive(uint8_t id, void *data, char *buf, size_t len) {
  assert(id == 0 && data == &storage && buf == storage.rx_queue);
  assert(len > 0 && len <= UART_RX_QUEUE_LEN / 2);
  assert(reads == callbacks + 1);
  for (size_t i = 0; i < len; i++) {
    assert(buf[i] == (char)('A' + callbacks));
  }
  callbacks++;
}
void vTaskDelay(TickType_t ticks) {
  assert(ticks == 10 / portTICK_PERIOD_MS);
  assert(callbacks == acknowledgements + 1);
  assert(reads == callbacks);
  delays++;
  // Do not acknowledge immediately: the driver must wait, without reading again.
  if (delays % 3 == 0) {
    hal_uart_rx_cb_done(0);
    acknowledgements++;
  }
}
static void reject_geometry(size_t len) {
  expect_assertion = true;
  if (setjmp(assertion_jump) == 0) {
    hal_uart_start_rx(0, receive, storage.rx_queue, len);
    fputs("invalid ESP32 RX buffer geometry was accepted\n", stderr);
    exit(1);
  }
  expect_assertion = false;
  assert(task_creations == 0);
}

int main(void) {
  size_t max_tx_len;
  hal_uart_init(0, 115200, &storage, &max_tx_len);
  assert(max_tx_len > 0);
  reject_geometry(0);
  reject_geometry(1);
  reject_geometry(3);
  hal_uart_start_rx(0, receive, storage.rx_queue, sizeof(storage.rx_queue));
  assert(task_creations == 1);
  bytes_left = UART_RX_QUEUE_LEN * 3 + 1;
  if (setjmp(task_done) == 0) {
    receive_task(receive_task_arg);
    assert(false);
  }
  assert(reads == 7 && callbacks == reads && acknowledgements == callbacks);
  assert(delays == acknowledgements * 3 && events == 1);
  printf("esp32: queue %u caps reads to a half-buffer and waits for every acknowledgement\n",
         UART_RX_QUEUE_LEN);
}
