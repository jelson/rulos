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

#pragma once

#include "core/hal.h"
#include "core/heap.h"
#include "core/queue.h"

typedef struct {
  CharQueue *q;
  Time reception_time_us;
} UartQueue_t;

#define UART_QUEUE_LEN 32

typedef void (*UARTSendDoneFunc)(void *callback_data);

typedef struct {
  HalUart handler;
  uint8_t initted;

  // receive
  char recvQueueStore[UART_QUEUE_LEN];
  UartQueue_t recvQueue;

  // send
  const char *out_buf;
  uint8_t out_len;
  uint8_t out_n;
  UARTSendDoneFunc send_done_cb;
  void *send_done_cb_data;
} UartState_t;

///////////////// application API
void uart_init(UartState_t *uart, uint32_t baud, r_bool stop2, uint8_t uart_id);
r_bool uart_read(UartState_t *uart, char *c);
UartQueue_t *uart_recvq(UartState_t *uart);
void uart_reset_recvq(UartQueue_t *uq);

r_bool uart_send(UartState_t *uart, const char *c, uint8_t len,
                 UARTSendDoneFunc, void *callback_data);
r_bool uart_busy(UartState_t *u);
