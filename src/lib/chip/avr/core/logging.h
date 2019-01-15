/************************************************************************
 *
 * This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
 * System -- the free, open-source operating system for microcontrollers.
 *
 * Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
 * May 2009.
 *
 * This operating system is in the public domain.  Copyright is waived.
 * All source code is completely free to use by anyone for any purpose
 * with no restrictions whatsoever.
 *
 * For more information about the project, see: www.jonh.net/rulos
 *
 ************************************************************************/

#pragma once

#include "core/hardware.h"
#include "core/util.h"

#ifdef LOG_TO_SERIAL

#define LOG(fmt, ...)                        \
  do {                                       \
    static const char fmt_p[] PROGMEM = fmt; \
    avr_log(fmt_p, ##__VA_ARGS__);           \
  } while (0)

#else  // LOG_TO_SERIAL

#define LOG(...)

#endif  // LOG_TO_SERIAL

#define assert(x)           \
  do {                      \
    if (!(x)) {             \
      avr_assert(__LINE__); \
    }                       \
  } while (0)
