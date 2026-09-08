// Host regression: compile with src/lib/periph/uart/uart.c and src/lib/core/queue.c,
// -Isrc/lib -Isrc/lib/chip/sim -ffunction-sections -Wl,--gc-sections.
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/clock.h"
#include "periph/uart/uart.h"
#include "periph/uart/uart_hal.h"

static hal_uart_receive_cb receive;
static void *hal_data;
static char *dma_buffer;
static ActivationFuncPtr pending;
static void *pending_data;
static unsigned acknowledgements;
static size_t expected_len;
static bool callback_active;
static bool callback_finished;
static bool expect_assertion;
static jmp_buf assertion_jump;

_Static_assert(sizeof(UartRxBuffer_t) == UART_RX_QUEUE_LEN * 3 / 2,
               "RX storage must be exactly one DMA buffer and one half-buffer snapshot");

void hal_uart_init(uint8_t id, uint32_t baud, void *data, size_t *max_tx_len) {
  (void)id;
  (void)baud;
  hal_data = data;
  *max_tx_len = 65535;
}
void hal_uart_start_rx(uint8_t id, hal_uart_receive_cb cb, void *buf, size_t buflen) {
  (void)id;
  assert(buflen == UART_RX_QUEUE_LEN);
  receive = cb;
  dma_buffer = buf;
}
void hal_uart_rx_cb_done(uint8_t id) {
  (void)id;
  UartState_t *uart = hal_data;
  assert(!callback_active && callback_finished);
  assert(uart->rx_pending_cb_len == 0);
  acknowledgements++;
}
void schedule_now(ActivationFuncPtr func, void *data) {
  assert(pending == NULL);
  pending = func;
  pending_data = data;
}
void log_assert(const char *file, unsigned long line) {
  if (expect_assertion) {
    longjmp(assertion_jump, 1);
  }
  fprintf(stderr, "%s:%lu: assertion failed\n", file, line);
  abort();
}
static void consumer(UartState_t *uart, void *data, char *buf, size_t len) {
  assert(data == &expected_len);
  assert(len == expected_len);
  assert(buf == uart->rx_storage->rx_pending_storage);
  assert(uart->rx_pending_cb_len == expected_len);
  callback_active = true;
  unsigned old_acknowledgements = acknowledgements;
  // The peripheral can also reuse its buffer while the task callback runs.
  memset(dma_buffer, 'D', UART_RX_QUEUE_LEN);
  for (size_t i = 0; i < len; i++) {
    assert(buf[i] == 'A');
  }
  assert(acknowledgements == old_acknowledgements);
  callback_active = false;
  callback_finished = true;
}

static void reject_delivery(size_t len) {
  expect_assertion = true;
  if (setjmp(assertion_jump) == 0) {
    receive(0, hal_data, dma_buffer, len);
    fputs("invalid HAL delivery was accepted\n", stderr);
    exit(1);
  }
  expect_assertion = false;
  assert(pending == NULL);
  assert(((UartState_t *)hal_data)->rx_pending_cb_len == 0);
}

static void reject_rebinding(UartState_t *uart) {
  UartRxBuffer_t replacement;
  UartRxBuffer_t *old_storage = uart->rx_storage;
  expect_assertion = true;
  if (setjmp(assertion_jump) == 0) {
    uart_start_rx(uart, &replacement, consumer, &expected_len);
    fputs("replacing active RX storage was accepted\n", stderr);
    exit(1);
  }
  expect_assertion = false;
  assert(uart->rx_storage == old_storage);
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--state-size") == 0) {
    printf("%zu\n", sizeof(UartState_t));
    return 0;
  }
  UartState_t uart;
  struct {
    unsigned before;
    UartRxBuffer_t rx;
    unsigned after;
  } storage = {.before = 0x12345678, .after = 0xabcdef01};
  uart_init(&uart, 0, 115200);
  assert(uart.rx_storage == NULL);
  uart_start_rx(&uart, &storage.rx, consumer, &expected_len);
  assert(dma_buffer == storage.rx.rx_queue);
  const size_t lengths[] = {1, UART_RX_QUEUE_LEN / 2 - 1, UART_RX_QUEUE_LEN / 2};
  for (unsigned i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
    expected_len = lengths[i];
    if (expected_len == 0) {
      continue;
    }
    unsigned old_acknowledgements = acknowledgements;
    callback_finished = false;
    memset(dma_buffer, 'A', expected_len);
    receive(0, hal_data, dma_buffer, expected_len);
    assert(acknowledgements == old_acknowledgements);
    assert(!callback_finished);
    uart_start_rx(&uart, &storage.rx, consumer, &expected_len);
    reject_rebinding(&uart);
    memset(dma_buffer, 'C', UART_RX_QUEUE_LEN);
    assert(pending != NULL);
    pending(pending_data);
    pending = NULL;
    assert(acknowledgements == old_acknowledgements + 1);
    assert(storage.before == 0x12345678 && storage.after == 0xabcdef01);
  }
  reject_delivery(0);
  reject_delivery(UART_RX_QUEUE_LEN / 2 + 1);
  reject_rebinding(&uart);
  printf("uart: queue %u retains bounded snapshots until callback acknowledgement\n",
         UART_RX_QUEUE_LEN);
}
