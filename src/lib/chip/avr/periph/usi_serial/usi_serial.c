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

#include <avr/io.h>

#include "chip/avr/core/usi_pins.h"
#include "hardware.h"

void usi_serial_send(const char* s) {
  gpio_make_output(USI_DO);
  gpio_make_output(USI_SCL);

  while (*s) {
    USIDR = *s++;

    // Clock out the bits
    for (int j = 0; j < 8; j++) {
      USICR = (1 << USIWM0) | (0 << USICS0) | (1 << USITC);
      USICR = (1 << USIWM0) | (0 << USICS0) | (1 << USITC) | (1 << USICLK);
    }
  }
}
