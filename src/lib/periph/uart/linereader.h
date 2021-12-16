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

#include "periph/uart/uart.h"

// Utility that reads from a serial port interrupt handlers, groups characters
// into complete lines (terminated by \r or \n), and produces user-context
// upcalls of each complete line received.
//
// Blank/empty lines do not generate upcalls.

typedef void (*linereader_cb)(UartState_t *uart, void *user_data, char *line);

#ifndef UART_RX_QUEUE_LEN
#define UART_RX_QUEUE_LEN 128
#endif

typedef struct {
  union {
    char storage[sizeof(CharQueue) + UART_RX_QUEUE_LEN];
    CharQueue q;
  } rx_queue;
  UartState_t *uart;
  linereader_cb cb;
  void *user_data;
  bool cb_pending;
} LineReader_t;

void linereader_init(LineReader_t *linereader, UartState_t *uart,
                     linereader_cb cb, void *user_data);
