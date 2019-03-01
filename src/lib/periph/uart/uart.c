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

#include <stdlib.h>

#include "core/clock.h"
#include "core/rulos.h"
#include "periph/uart/uart.h"

// Upcall from HAL when new data arrives.  Happens at interrupt time.
void _uart_receive(HalUart *handler, char c) {
  UartState_t *u = (UartState_t *)handler;
  if (!u->initted) return;

  if (u->recvQueue.reception_time_us == 0) {
    u->recvQueue.reception_time_us = precise_clock_time_us();
  }

#ifdef UART_DEBUG
  LOG("uart_receive: got char at %d, msgtime=%d", precise_clock_time_us(),
      u->recvQueue.reception_time_us);
#endif

  // safe because we're in interrupt time.
  CharQueue_append(u->recvQueue.q, c);
}

// Upcall from hal when the next byte is needed for a send.  Happens
// at interrupt time.
r_bool _uart_get_next_character(HalUart *handler, char *c /* OUT */) {
  UartState_t *u = (UartState_t *)handler;
  assert(u != NULL);

  if (u->out_buf == NULL) {
    return FALSE;
  }

  *c = u->out_buf[u->out_n++];

  if (u->out_n == u->out_len) {
    if (u->send_done_cb != NULL) {
      // Call the "done" upcall - N.B., at interrupt time
      u->send_done_cb(u->send_done_cb_data);
    }
    u->out_buf = NULL;
    u->send_done_cb = NULL;
    u->send_done_cb_data = NULL;
  }

  return TRUE;
}

//////////////////////////////////////////////////////////

r_bool uart_read(UartState_t *u, char *c /* OUT */) {
  rulos_irq_state_t old_interrupts = hal_start_atomic();
  r_bool retval = CharQueue_pop(u->recvQueue.q, c);
  hal_end_atomic(old_interrupts);
  return retval;
}

UartQueue_t *uart_recvq(UartState_t *u) {
  if (!u->initted) return NULL;

  return &u->recvQueue;
}

void uart_reset_recvq(UartQueue_t *uq) {
  uq->reception_time_us = 0;
  CharQueue_clear(uq->q);
}

r_bool uart_send(UartState_t *u, const char *c, uint8_t len,
                 UARTSendDoneFunc callback, void *callback_data) {
  assert(u->initted);

  if (u->out_buf != NULL || len == 0) return FALSE;

  u->out_buf = c;
  u->out_len = len;
  u->out_n = 0;
  u->send_done_cb = callback;
  u->send_done_cb_data = callback_data;

  hal_uart_start_send(&u->handler);
  return TRUE;
}

r_bool uart_busy(UartState_t *u) { return (u->out_buf != NULL); }

void uart_init(UartState_t *u, uint32_t baud, r_bool stop2, uint8_t uart_id) {
  u->handler.send = _uart_get_next_character;
  u->handler.recv = _uart_receive;

  // initialize the queue
  u->recvQueue.q = (CharQueue *)u->recvQueueStore;
  CharQueue_init(u->recvQueue.q, sizeof(u->recvQueueStore));
  u->recvQueue.reception_time_us = 0;
  hal_uart_init(&u->handler, baud, stop2, uart_id);
  u->out_buf = NULL;
  u->initted = TRUE;
}
