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

#include "core/logging-common.h"

void arm_assert(const char *file, uint32_t line);
void arm_log(const char *fmt, ...) __attribute((format(printf, 1, 2)));

#undef assert

#define assert(x)                     \
  do {                                \
    if (!(x)) {                       \
      arm_assert(__FILE__, __LINE__); \
    }                                 \
  } while (0)

#ifdef LOG_TO_SERIAL

#define LOG(fmt, ...)                     \
  do {                                    \
    static const char fmt_p[] = fmt "\n"; \
    arm_log(fmt_p, ##__VA_ARGS__);        \
  } while (0)

#define LOG_FLUSH()     \
  do {                  \
    log_common_flush(); \
  } while (0);

#else  // LOG_TO_SERIAL

#define LOG(...)
#define LOG_FLUSH(...)

#endif  // LOG_TO_SERIAL
