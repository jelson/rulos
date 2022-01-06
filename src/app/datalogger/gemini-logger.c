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
#include "curr_meas.h"
#include "flash_dumper.h"
#include "periph/uart/uart.h"
#include "serial_reader.h"

// uart definitions. Note that, other than the board's console, we
// only use the RX side of the other UARTs. It's confusing because
// some uarts have names like "PSOC TX" and "PSOC RX", but that means
// we're snooping on both the RX and TX sides of the other device's
// serial port, using two of our RX ports.
#define PSOC_RX_UART_NUM 2
#define PSOC_TX_UART_NUM 0
#define ISP_RX_UART_NUM 3
#define ISP_TX_UART_NUM 1

#define CONSOLE_UART_NUM 4

UartState_t console;
flash_dumper_t flash_dumper;
serial_reader_t psoc_rx, psoc_tx, isp_rx, isp_tx;

static void indicate_alive(void *data) {
  LOG("run");
  schedule_us(1000000, indicate_alive, NULL);
}

int main() {
  rulos_hal_init();
  init_clock(10000, TIMER1);

  // initialize console uart
  uart_init(&console, CONSOLE_UART_NUM, 1000000);
  log_bind_uart(&console);
  LOG("Gemini Datalogger starting");

  // initialize flash dumper
  flash_dumper_init(&flash_dumper);

  // initialize serial readers
  serial_reader_init(&psoc_rx, PSOC_RX_UART_NUM, 1000000, &flash_dumper, NULL);
  serial_reader_init(&psoc_tx, PSOC_TX_UART_NUM, 1000000, &flash_dumper, NULL);
  serial_reader_init(&isp_rx, ISP_RX_UART_NUM, 500000, &flash_dumper, NULL);
  serial_reader_init(&isp_tx, ISP_RX_UART_NUM, 500000, &flash_dumper, NULL);

  // enable periodic blink to indicate liveness
  schedule_now(indicate_alive, NULL);

  flash_dumper_print(&flash_dumper, "restarting\n\n\n");
  flash_dumper_print(&flash_dumper, "startup");
  cpumon_main_loop();
}
