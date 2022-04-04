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

#include "core/hardware.h"
#include "core/rulos.h"
#include "periph/uart/linereader.h"
#include "periph/uart/uart.h"

#define JIFFY_CLOCK_US 10000  // 10 ms jiffy clock

static void command_received(UartState_t *uart, void *data, char *line) {
  LOG("Received a command via serial: '%s'", line);
}

int main() {
  rulos_hal_init();
  init_clock(JIFFY_CLOCK_US, TIMER0);

  UartState_t u;
  uart_init(&u, /* uart_id= */ 0, 1000000);
  log_bind_uart(&u);

  LineReader_t lr;
  linereader_init(&lr, &u, command_received, NULL);

  LOG("Console command reader is waiting for your commands.");
  scheduler_run();
}
