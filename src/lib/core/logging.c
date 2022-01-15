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

#include "core/logging.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "core/hal.h"
#include "periph/uart/uart.h"

static UartState_t *logging_uart = NULL;

// Set up logging system to emit log messages to a particular uart.
void log_bind_uart(UartState_t *u) {
  logging_uart = u;
}

void log_write(const void *buf, size_t len) {
  if (logging_uart != NULL) {
    uart_write(logging_uart, buf, len);
  }
}

void log_flush() {
  if (logging_uart != NULL) {
    rulos_uart_flush(logging_uart);
  }
}

void log_format_and_write(const char *fmt, ...) {
  va_list ap;
  char message[120];
  va_start(ap, fmt);

  // Under RULOS, the static strings are in progmem, so they need a special
  // version of printf
#ifdef RULOS_AVR
  int len = vsnprintf_P(message, sizeof(message), fmt, ap);
#else
  int len = vsnprintf(message, sizeof(message), fmt, ap);
#endif
  message[sizeof(message) - 1] = '\0';
  va_end(ap);

  log_write(message, len);
}

void log_assert(const char *file, const long unsigned int line) {
#if LOG_TO_SERIAL
  log_format_and_write("assertion failed: file %s, line %lu", file, line);
  log_flush();
#endif
  __builtin_trap();
}
