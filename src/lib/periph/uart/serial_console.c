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

#include <stdlib.h>

#include "core/clock.h"
#include "periph/uart/serial_console.h"

void serial_console_update(SerialConsole *sca) {
  char rcv_chr;
  if (CharQueue_pop(sca->uart.recvQueue.q, &rcv_chr)) {
    *(sca->line_ptr) = rcv_chr;

    sca->line_ptr += 1;
    if (sca->line_ptr == &sca->line[sizeof(sca->line) - 1]) {
      // Buffer full!
      rcv_chr = '\n';
    }
    if (rcv_chr == '\n') {
      *(sca->line_ptr) = '\0';
      sca->line_ptr = sca->line;
      if (sca->line_act.func != NULL) {
        (sca->line_act.func)(sca->line_act.data);
      }
    }
  }

  schedule_us(120, (ActivationFuncPtr)serial_console_update, sca);
}

void serial_console_init(SerialConsole *sca, ActivationFuncPtr line_func,
                         void *line_data) {
  uart_init(&sca->uart, 38400, TRUE, 0);
  sca->line_act.func = line_func;
  sca->line_act.data = line_data;

  schedule_us(1000, (ActivationFuncPtr)serial_console_update, sca);
}

void serial_console_sync_send(SerialConsole *act, const char *buf,
                              uint16_t buflen) {
  while (uart_busy(&act->uart)) {
  }
  uart_send(&act->uart, buf, buflen, NULL, NULL);
  while (uart_busy(&act->uart)) {
  }
}
