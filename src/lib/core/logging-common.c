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

#include <stddef.h>

#include "core/hal.h"
#include "core/logging.h"
#include "periph/uart/uart.h"

static UartState_t *logging_uart = NULL;

// Set up logging system to emit log messages to a particular uart.
void log_bind_uart(UartState_t *u) {
  logging_uart = u;
}

void log_common_emit_to_bound_uart(const char *s, int len) {
  if (logging_uart != NULL) {
    uart_write(logging_uart, s, len);
  }
}

void log_common_flush() {
  if (logging_uart != NULL) {
    uart_flush(logging_uart);
  }
}
