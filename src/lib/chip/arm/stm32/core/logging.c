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

/*
 * hardware.c: These functions are only needed for physical display hardware.
 *
 * This file is not compiled by the simulator.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "core/hal.h"
#include "core/logging.h"

#ifdef LOG_TO_SERIAL

// This silliness is because "gcc -DLOG_TO_SERIAL" defaults to
// defining LOG_TO_SERIAL to be 1, which is a confusing default.
// I want to force you to explicitly name either UART0 or UART1
// as your desired logging destination.

#define UART0 100
#define UART1 200

#if LOG_TO_SERIAL == UART0
#define LOGGING_UART 0
#elif LOG_TO_SERIAL == UART1
#define LOGGING_UART 1
#else
#error LOG_TO_SERIAL must be set to UART0 or UART1
#include <stophere>
#endif

#undef UART0
#undef UART1

void arm_log(const char *fmt, ...) {
  va_list ap;
  char message[100];
  va_start(ap, fmt);
  vsnprintf(message, sizeof(message), fmt, ap);
  va_end(ap);

  // This is a hardware-specific function.
  arm_uart_sync_send_by_id(LOGGING_UART, message);
}

#endif  // LOG_TO_SERIAL
