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
 * Logging support for ARM devices. (This can not be shared by AVR devices
 * because on AVR we use a special version of printf that can print from AVR
 * progmem regions.)
 */

#include "core/logging.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "core/logging-common.h"

void arm_log(const char *fmt, ...) {
  va_list ap;
  char message[120];
  va_start(ap, fmt);
  int len = vsnprintf(message, sizeof(message), fmt, ap);
  message[sizeof(message) - 1] = '\0';
  va_end(ap);

  log_common_emit_to_bound_uart(message, len);
}

void arm_assert(const char *file, const uint32_t line) {
  LOG("assertion failed: file %s, line %lu", file, line);
  LOG_FLUSH();
  __builtin_trap();
}
