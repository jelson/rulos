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

/*
 * This file maps the USI_SDA and USI_SCL macros to specific pins, depending
 * on chip type.
 */

#pragma once

#include "hardware.h"

#if \
  defined (__AVR_ATtiny24__)  | \
  defined (__AVR_ATtiny24A__) | \
  defined (__AVR_ATtiny44__)  | \
  defined (__AVR_ATtiny44A__) | \
  defined (__AVR_ATtiny84__)  | \
  defined (__AVR_ATtiny84A__)
#define USI_SDA GPIO_A6
#define USI_DO  GPIO_A5
#define USI_SCL GPIO_A4
#elif \
  defined (__AVR_AT90Tiny2313__) | \
  defined (__AVR_ATTiny2313__)
#define USI_SDA GPIO_B5
#define USI_DO  GPIO_B6
#define USI_SCL GPIO_B7
#elif \
  defined(__AVR_ATtiny26__) | \
  defined(__AVR_ATtiny25__) | \
  defined(__AVR_ATtiny45__) | \
  defined(__AVR_ATtiny85__)
#define USI_SDA GPIO_B0
#define USI_DO  GPIO_B1
#define USI_SCL GPIO_B2
#else
# error USI pin definitions needed for this chip
#endif
