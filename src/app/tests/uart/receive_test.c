// Host regression: compile with src/lib/periph/uart/uart.c and src/lib/core/queue.c,
// -Isrc/lib -Isrc/lib/chip/sim -ffunction-sections -Wl,--gc-sections.
#include <assert.h>
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
  acknowledgements++;
}
void schedule_now(ActivationFuncPtr func, void *data) {
  assert(pending == NULL);
  pending = func;
  pending_data = data;
}
void log_assert(const char *file, unsigned long line) {
  fprintf(stderr, "%s:%lu: assertion failed\n", file, line);
  abort();
}
static void consumer(UartState_t *uart, void *data, char *buf, size_t len) {
  (void)uart;
  (void)data;
  assert(len == expected_len);
  for (size_t i = 0; i < len; i++) {
    assert(buf[i] == 'A');
  }
}
int main(void) {
  UartState_t uart;
  uart_init(&uart, 0, 115200);
  uart_start_rx(&uart, consumer, NULL);
  const size_t lengths[] = {1, UART_RX_QUEUE_LEN / 2, UART_RX_QUEUE_LEN};
  for (unsigned i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
    expected_len = lengths[i];
    memset(dma_buffer, 'A', expected_len);
    receive(0, hal_data, dma_buffer, expected_len);
    assert(acknowledgements == i);
    memset(dma_buffer, 'C', UART_RX_QUEUE_LEN);
    assert(pending != NULL);
    pending(pending_data);
    pending = NULL;
    assert(acknowledgements == i + 1);
  }
  puts("uart: deferred callbacks retain data across RX buffer reuse");
}
