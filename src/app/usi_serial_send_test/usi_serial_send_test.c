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

/////////////////////////////

#include "chip/avr/periph/usi_serial/usi_serial.h"
#include "core/rulos.h"

int main() {
  hal_init();

  while (true) {
    for (int i = 0; i < 10000; i++) {
      char buf[50];
      sprintf(buf, "This is test number %05d", i);
      usi_serial_send(buf);
    }
  }
}
