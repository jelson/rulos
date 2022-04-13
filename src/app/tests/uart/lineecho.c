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

#ifdef RULOS_ESP32
#define DUT_UART       1
#define DUT_UART_SPEED 9600
#endif

typedef struct {
  UartState_t uart;
  LineReader_t lr;
  int line_num;
} serial_state;

serial_state console = {};
serial_state dut = {};

static void line_received(UartState_t *uart, void *user_data, char *line) {
  serial_state *ss = (serial_state *)user_data;
  LOG("uart %d got line %d: ", ss->uart.uart_id, ss->line_num++);
  log_write(line, strlen(line));
  log_write("\n", 1);
#ifdef RULOS_ARM_STM32
  // hal_uart_log_stats(uart->uart_id);
#endif
}

int main() {
  rulos_hal_init();

  uart_init(&console.uart, /* uart_id= */ 0, 1000000);
  log_bind_uart(&console.uart);
  LOG("lineecho up and running");

  linereader_init(&console.lr, &console.uart, line_received, &console);

#if DUT_UART
  uart_init(&dut.uart, /* uart_id= */ DUT_UART, DUT_UART_SPEED);
  linereader_init(&dut.lr, &dut.uart, line_received, &dut);
  LOG("reading from dut, uart %d, too!", DUT_UART);
#endif

  init_clock(10000, TIMER1);
  scheduler_run();
}
