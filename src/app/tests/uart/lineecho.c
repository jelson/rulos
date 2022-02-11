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

UartState_t uart, gps_uart;
LineReader_t linereader, gps_linereader;
int i = 0;

static void line_received(UartState_t *uart, void *user_data, char *line) {
  //  if (i % 10 == 0) {
#if 0
  if (true) {
    for (int j = 0; j < 10; j++) {
      LOG("got line %d:%d", i, j);
    }
  }
  i++;
#else
  LOG("uart %d got line %d: '%s'", uart->uart_id, i++, line);
#endif
}

#define USE_GPS 0

int main() {
  rulos_hal_init();

  uart_init(&uart, /* uart_id= */ 0, 38400);
  log_bind_uart(&uart);
  linereader_init(&linereader, &uart, line_received, &uart);
  LOG("lineecho up and running");

#if USE_GPS
  uart_init(&gps_uart, /* uart_id= */ 4, 9600, true);
  linereader_init(&gps_linereader, &gps_uart, line_received, &gps_uart);
  LOG("reading from gps too!");
#endif

  init_clock(10000, TIMER1);

  scheduler_run();
}
