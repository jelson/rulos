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
#include "periph/uart/uart.h"

UartState_t uart;

void char_received(UartState_t *s, void *user_data, char c) {
  LOG("got char: '%c'", c);
}

int main() {
  rulos_hal_init();

  uart_init(&uart, /* uart_id= */ 0, 38400);
  uart_start_rx(&uart, char_received, NULL);
  log_bind_uart(&uart);
  LOG("uartecho up and running");

  init_clock(10000, TIMER1);

  cpumon_main_loop();
}
