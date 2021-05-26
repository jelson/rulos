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

#include <stdbool.h>
#include <stdint.h>

#include "core/queue.h"

#ifndef UART_TX_QUEUE_LEN
#define UART_TX_QUEUE_LEN 128
#endif

struct UartState_t_s;
typedef struct UartState_t_s UartState_t;

// Upcall for reception. Called at interrupt-time.
typedef void (*uart_rx_cb)(UartState_t *s, void *user_data, char c);

struct UartState_t_s {
  bool initted;
  uint8_t uart_id;
  uint16_t max_tx_len;

  // send
  union {
    char storage[sizeof(CharQueue) + UART_TX_QUEUE_LEN];
    CharQueue q;
  } tx_queue;
  uint16_t pending_tx_len;  // num of bytes being sent by the hal
  // needed separately from pending_tx_len so that we don't double-launch a
  // write train if a second write arrives before the upcall due to the first
  bool writes_active;

  // receive
  uart_rx_cb rx_cb;
  void *rx_user_data;
};

///////////////// application API

// initialize an instance of a uart
void uart_init(UartState_t *u, uint8_t uart_id, uint32_t baud, bool stop2);

// Registers a char-received callback to be called at interrupt time each time a
// character arrives.
void uart_start_rx(UartState_t *u, uart_rx_cb rx_cb, void *user_data);

// Sends binary data to the UART, specified with a length.
void uart_write(UartState_t *u, const void *buf, uint8_t len);

// Sends a null-terminated string to the UART.
void uart_print(UartState_t *u, const char *s);

// Returns true if data is still being transmitted.
bool uart_is_busy(UartState_t *u);

// Waits until the uart is completely flushed.
void uart_flush(UartState_t *u);
