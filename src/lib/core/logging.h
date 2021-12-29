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

#include <stdint.h>

#include "periph/uart/uart.h"

// Set up logging system to emit log messages to a particular uart.
void log_bind_uart(UartState_t *u);

// Emit to log system. Not really meant to be called directly, but through the
// LOG() macro.
void log_write(const void *buf, int len);

// wait until log is drained
void log_flush();

// typically not called directly, but via the LOG macro
void log_format_and_write(const char *fmt, ...)
    __attribute((format(printf, 1, 2)));

// typically noot called directly, but via the assert() macro
void log_assert(const char *file, long unsigned int line);

#undef assert

#define assert(x)                     \
  do {                                \
    if (!(x)) {                       \
      log_assert(__FILE__, __LINE__); \
    }                                 \
  } while (0)

#ifdef LOG_TO_SERIAL

// We want to add the "PROGMEM" modifier on AVR platforms so that log messages
// end up in program memory, not ram. On other platforms, define it as a no-op
#ifndef PROGMEM
#define PROGMEM
#endif

#define LOG(fmt, ...)                             \
  do {                                            \
    static const char fmt_s[] PROGMEM = fmt "\n"; \
    log_format_and_write(fmt_s, ##__VA_ARGS__);   \
  } while (0)

#else  // LOG_TO_SERIAL

#define LOG(...)

#endif  // LOG_TO_SERIAL
