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

#include "periph/uart/uart.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "core/queue.h"
#include "core/rulos.h"
#include "core/util.h"
#include "periph/uart/uart_hal.h"

// XXX TODO HELPME:
//
// This uses the RULOS Queue class to keep the send queue. It has linear-time
// pop, which means that sending strings on the AVR, which can only consume one
// character at a time, takes n^2 time. We should change this to use a circular
// buffer. To do so, the ringbuf class will have to be modified to give a
// pointer to the longest contiguous segment of the buffer, for DMA purposes.
//

//// reception

// Data comes up from the UART's HAL layer at interrupt time. We convert it into
// a task-time callback, invoked from here.
static void _uart_receive_trampoline(void *data) {
  UartState_t *u = (UartState_t *)data;

  // call the user callback
  u->rx_cb(u, u->rx_user_data, u->rx_pending_cb_buf, u->rx_pending_cb_len);

  // indicate to the HAL that we're ready for the next callback
  u->rx_pending_cb_buf = NULL;
  u->rx_pending_cb_len = 0;
  hal_uart_rx_cb_done(u->uart_id);

  // report an overflow, if we recorded one during the interrupt handler
  if (u->rx_overflow_bytes != u->rx_overflow_bytes_last_reported) {
    LOG("WARNING: uart %u dropped %" PRIu32 " bytes (%" PRIu32 " total)",
        u->uart_id,
        u->rx_overflow_bytes - u->rx_overflow_bytes_last_reported,
        u->rx_overflow_bytes);
    u->rx_overflow_bytes_last_reported = u->rx_overflow_bytes;
  }
}

// Upcall from HAL when new data arrives.  Happens at interrupt time.
static void _uart_receive(uint8_t uart_id, void *user_data, char *buf,
                          size_t len) {
  UartState_t *u = (UartState_t *)user_data;
  assert(u != NULL);
  assert(u->uart_id == uart_id);
  assert(u->initted);

  // hal should never give us an RX callback unless we called down to say the
  // previous one was complete
  assert(u->rx_pending_cb_buf == NULL);

  u->rx_pending_cb_buf = buf;
  u->rx_pending_cb_len = len;
  schedule_now(_uart_receive_trampoline, u);
}

void uart_start_rx(UartState_t *u, uart_rx_cb rx_cb, void *user_data) {
  u->rx_cb = rx_cb;
  u->rx_user_data = user_data;
  hal_uart_start_rx(u->uart_id, _uart_receive, &u->rx_queue, UART_RX_QUEUE_LEN);
}

//// sending

// Upcall from hal when the next byte is needed for a send.  Happens
// at interrupt time.
static void _get_next_data(uint8_t uart_id, void *user_data,
                           const char **tx_buf /*OUT*/,
                           uint16_t *tx_len /*OUT*/) {
  UartState_t *u = (UartState_t *)user_data;
  assert(u != NULL);
  assert(u->initted);
  assert(u->uart_id == uart_id);
  assert(u->writes_active);

  // If there was a pending tx, this serves as an ack; the old data can be
  // popped.
  if (u->pending_tx_len > 0) {
    CharQueue_pop_n(&u->tx_queue.q, NULL, u->pending_tx_len);
  }

  // determine how much data should be transmitted in the next batch
  u->pending_tx_len = r_min(CharQueue_length(&u->tx_queue.q), u->max_tx_len);

  if (u->pending_tx_len == 0) {
    *tx_buf = NULL;
    *tx_len = 0;
    u->writes_active = false;
  } else {
    *tx_buf = CharQueue_ptr(&u->tx_queue.q);
    *tx_len = u->pending_tx_len;
  }
}

void uart_write(UartState_t *u, const void *buf, size_t len) {
  const char *c = (char *)buf;
  assert(u->initted);

  while (len > 0) {
    bool should_start_write = false;

    // In critical section: add as much new data to the send queue as will fit.
    rulos_irq_state_t old_interrupts = hal_start_atomic();
    uint16_t write_size =
        r_min((size_t)len, CharQueue_free_space(&u->tx_queue.q));

    if (write_size == 0) {
      // If there's more data that did not fit in the queue, block until there's
      // free space. This is a policy choice: if dropping data is better for
      // some application than blocking, there could be two variants of
      // uart_write with different behavior here.
      hal_end_atomic(old_interrupts);

      // Note that we should *not* just call hal_idle here: we end up spinning
      // too quickly, disable interrupts for too long
      delay_us(20000);
      continue;
    }

    assert(true == CharQueue_append_n(&u->tx_queue.q, c, write_size));
    c += write_size;
    len -= write_size;

    // If there isn't already a write pending, remember that we have to launch
    // one.
    if (!u->writes_active) {
      u->writes_active = true;
      should_start_write = true;
    }
    hal_end_atomic(old_interrupts);

    // If the write-train was idle, start a new one
    if (should_start_write) {
      hal_uart_start_send(u->uart_id, _get_next_data);
    }
  }
}

void uart_print(UartState_t *u, const char *s) {
  uart_write(u, s, strlen(s));
}

// returns true if there's anything in the buffer
bool uart_is_busy(UartState_t *u) {
  assert(u->initted);
  return CharQueue_length(&u->tx_queue.q) > 0 || u->writes_active;
}

void rulos_uart_flush(UartState_t *u) {
  while (uart_is_busy(u)) {
    hal_idle();
  }
}

void uart_init(UartState_t *u, uint8_t uart_id, uint32_t baud) {
  memset(u, 0, sizeof(*u));
  u->uart_id = uart_id;
  CharQueue_init(&u->tx_queue.q, sizeof(u->tx_queue));
  hal_uart_init(uart_id, baud, u, &u->max_tx_len);
  u->initted = true;
}
