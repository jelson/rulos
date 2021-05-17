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
#include "periph/uart/linereader.h"

UartState_t uart;
LineReader_t linereader;
int i = 0;

static void line_received(void *user_data, char *line) {
  //  if (i % 10 == 0) {
#if 0
  if (true) {
    for (int j = 0; j < 10; j++) {
      LOG("got line %d:%d", i, j);
    }
  }
  i++;
#else
  LOG("got line %d: '%s'", i++, line);
#endif
}

int main() {
  hal_init();

  uart_init(&uart, /* uart_id= */ 0, 38400, true);
  log_bind_uart(&uart);
  linereader_init(&linereader, &uart, line_received, NULL);
  LOG("lineecho up and running!");

  init_clock(10000, TIMER1);

  cpumon_main_loop();
}
