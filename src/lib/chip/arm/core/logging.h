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
    static const char fmt_p[] = fmt; \
    arm_log(fmt_p, ##__VA_ARGS__);           \
  } while (0)

#else  // LOG_TO_SERIAL

#define LOG(...)

#endif  // LOG_TO_SERIAL

//arm_assert(__LINE__);
//    char buf[100];      
// char *p =NULL;         
//    sprintf(buf, "hi there %p", p);               
//*p = buf[0]; 

#include <stdio.h>

#define assert(x)           \
  do {                      \
    if (!(x)) {             \
    do {} while (1); \
    }                       \
  } while (0)

