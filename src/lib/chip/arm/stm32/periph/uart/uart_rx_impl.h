// Private RX delivery implementation. Include after defining stm32_uart_t and the UART table.
#pragma once

static void rx_send_up(uint8_t uart_id, stm32_uart_t *u, char *buf, size_t len) {
  assert(u->rx_cb_ready);
  assert(len <= u->rx_half_buflen);
  if (len > 0) {
    u->rx_cb_ready = false;
    u->rx_cb(uart_id, u->user_data, buf, len);
    if (len > u->max_chars_per_rx_batch) {
      u->max_chars_per_rx_batch = len;
    }
  }
}

#if USE_RX_DMA_FOR_CHIP

// Deliver to a completed half-boundary and advance even when data must be dropped. Runs in DMA
// ISR context; DMA never stops, so no callback may retain the hardware buffer after returning.
static void rx_dma_deliver(uint8_t uart_id, uint32_t boundary, uint32_t next_pos) {
  stm32_uart_t *u = &g_stm32_uarts[uart_id];
  if (!u->initted) {
    return;
  }
  u->tot_ints++;

  uint32_t last = u->rx_dma_processed_pos;
  if (last >= boundary) {
    u->rx_dma_processed_pos = next_pos;
    return;
  }

  size_t len = boundary - last;
  u->tot_rx_bytes += len;
  u->rx_dma_processed_pos = next_pos;

  // TC may precede HT when both flags are pending after a wrap. If the old cursor is still in the
  // first half, that TC span includes overwritten data and cannot be reconstructed. Drop it so
  // the following HT can deliver the newest completed half, within the upper layer's snapshot.
  if (len > u->rx_half_buflen || !u->rx_cb_ready) {
    u->dropped_rx_bytes += len;
  } else {
    rx_send_up(uart_id, u, u->rx_buf + last, len);
  }
}

static void hal_uart_on_rx_dma_ht(void *user_data) {
  uint8_t uart_id = (uint8_t)(uintptr_t)user_data;
  stm32_uart_t *u = &g_stm32_uarts[uart_id];
  rx_dma_deliver(uart_id, u->rx_half_buflen, u->rx_half_buflen);
}

static void hal_uart_on_rx_dma_tc(void *user_data) {
  uint8_t uart_id = (uint8_t)(uintptr_t)user_data;
  stm32_uart_t *u = &g_stm32_uarts[uart_id];
  rx_dma_deliver(uart_id, 2 * u->rx_half_buflen, 0);
}

// IDLE flushes only the current half. The caller holds the interrupt mask and has checked that
// the previous receive callback is complete; HT/TC retain ownership of half-boundary transitions.
static void rx_dma_flush(uint8_t uart_id, stm32_uart_t *u, uint32_t remaining) {
  uint32_t buflen = 2 * u->rx_half_buflen;
  uint32_t current_pos = buflen - remaining;
  if (current_pos >= buflen) {
    current_pos = 0;
  }

  uint32_t last = u->rx_dma_processed_pos;
  uint32_t limit = (last < u->rx_half_buflen) ? u->rx_half_buflen : buflen;
  if (current_pos > limit) {
    current_pos = limit;
  }

  size_t len = 0;
  if (current_pos > last) {
    len = current_pos - last;
    u->tot_rx_bytes += len;
    u->rx_dma_processed_pos = current_pos;
  }
  u->rx_data_ready = false;
  if (len > 0) {
    rx_send_up(uart_id, u, u->rx_buf + last, len);
  }
}

#endif
